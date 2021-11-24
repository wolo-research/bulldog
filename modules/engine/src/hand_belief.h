//
// Created by Isaac Zhang on 4/7/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_HAND_BELIEF_H_
#define BULLDOG_MODULES_ENGINE_SRC_HAND_BELIEF_H_

#include <map>
#include <vector>
#include <set>
#include <bulldog/logger.hpp>
#include <bulldog/card_util.h>
#include <limits>
#include <cmath>
#include "engine_util.h"
/**
 * About hand value, or hand state
 * - hand value is a hand expected return of all opp hand states, from terminal to root.
 * - When the opponents takes an action, that the belief distribution is updated via Bayes’s rule
 */

const double BELIEF_VECTOR_1081_DEFAULT = 1.0 / 1081.0;
const double BELIEF_VECTOR_1326_DEFAULT = 1.0 / 1326.0;
const int FULL_HAND_BELIEF_SIZE = HOLDEM_MAX_HANDS_PERMUTATION;

static const double BELIEF_MASK_VALUE = -1;
struct sHandBelief {
  sHandBelief() {
    SetAll(BELIEF_VECTOR_1326_DEFAULT);
  }
  explicit sHandBelief(double v) {
    SetAll(v);
  }
  explicit sHandBelief(sHandBelief *that) {
    CopyValue(that);
  }

  double belief_[FULL_HAND_BELIEF_SIZE]{};

  VectorIndex SampleHand(unsigned long &x, unsigned long &y, unsigned long &z) {
    return RndXorShift<double>(belief_, FULL_HAND_BELIEF_SIZE, x, y, z, (1 << 16));
  }

  void CleanCrashHands(VectorIndex v) {
    for (int i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
      if (VectorIdxClash(v, i))
        Zero(i);
  }

  void Zero(int idx) {
    if (IsPruned(idx)) {
      return;
    }
    belief_[idx] = 0;
  }

  /*
   * in vectorized worker, rollout may make the belief to become super small.
   * using epsilon with 10^-10 may not be secure.
   * comparing to 0 should be safe.
   */
  bool IsZero(int idx) {
//    return fabs(belief_[idx] - 0.0) < DOUBLE_EPSILON;
    return belief_[idx] == 0;
  }

  void Prune(int idx) {
    belief_[idx] = BELIEF_MASK_VALUE;
  }
  bool IsPruned(int idx) {
    return fabs(belief_[idx] - BELIEF_MASK_VALUE) < DOUBLE_EPSILON;
  }

  //-1 also consdier 0 in this case. normally used in ranges propogations
  bool AllZero() {
    for (double &i : belief_)
      if (i > 0.0)
        return false;
    return true;
  }

  //normally used in range propogation. assuming all > 0 except -1
  bool AllPruned() {
    for (double &i : belief_)
      if (i > BELIEF_MASK_VALUE)
        return false;
    return true;
  }

  //todo test
  //normally used in range propogation. assuming all > 0 except -1
  void Normalize() {
    double sum = BeliefSum();
    if (sum == 0) {
      logger::warn("hand belief sum is zero, hack to 1.0/1326");
      for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
        if (!IsPruned(i))
          belief_[i] = 1.0 / FULL_HAND_BELIEF_SIZE;
    } else {
      double scaler = 1.0 / sum;
      for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
        if (!IsPruned(i))
          belief_[i] *= scaler;
    }
  }
  void PrintNonZeroBelief();

  void ExcludeBoard(Board_t &board){
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
      //if crash with board, set to 0 and continues
      auto high_low = FromVectorIndex(i);
      if (board.CardCrash(high_low.first) || board.CardCrash(high_low.second)) {
        Prune(i);
        continue;
      }
    }
  }
  void NormalizeExcludeBoard(Board_t &board) {
    ExcludeBoard(board);
    Normalize();
  }

  double BeliefSum() {
    double sum = 0;
    for (int i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
      if (!IsPruned(i)) //skip masked value -1 only
        sum += belief_[i];
    return sum;
  }

  bool HandEquals(sHandBelief *that) {
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
      if (!IsPruned(i)) {
        auto a = belief_[i];
        auto b = that->belief_[i];
        if (std::fabs(a - b) >= 0.0001) {
          logger::debug("!!!!! this = %.15f, while that = %.15f || idx = %d",
                        a,
                        b,
                        i);
          return false;
        }
      }
    }
    return true;
  }

  //set all hands , overriding -1
  void SetAll(double nv) {
    for (double &i : belief_)
      i = nv;
  };
  void DotMultiply(sHandBelief *that) {
    //they must be topo same
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
      if (!IsPruned(i) && !that->IsPruned(i))
        belief_[i] *= that->belief_[i];
  };

  int CountPrunedEntries() {
    int count = 0;
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
      if (IsPruned(i))
        count++;
    return count;
  }

  //copy everthing including the mask value.
  void CopyValue(sHandBelief *that) {
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
      belief_[i] = that->belief_[i];
  };

  void Scale(double factor) {
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
      if (!IsPruned(i)) //skip 0 and -1
        belief_[i] *= factor;
  }

  //delete all entries < 10^-6. an average range is about 1 / 1326 ~= 8 * 10^-4
  //normally used in range estimation. expect > 0 except -1
  void Purify();

  //used in sgs range estimate. it should be postive only. so skipping both 0 and 1 is fine
  int NonZeroBeliefCount();
  //all pruned should aligned
  bool TopoAligned(sHandBelief *that);
  void SetAllUnmaskedHands(double v);
};

struct Ranges {
  explicit Ranges(int players) {
    num_player_ = players;
    beliefs_ = new sHandBelief[players];
  }
  explicit Ranges(Ranges *that) {
    num_player_ = that->num_player_;
    beliefs_ = new sHandBelief[num_player_];
    Copy(that);
  }

  //only empty now
  Ranges(Ranges *that, const std::string &directive) {
    if (directive == "empty") {
      num_player_ = that->num_player_;
      beliefs_ = new sHandBelief[num_player_];
      Copy(that);
      for (int p = 0; p < num_player_; p++)
        beliefs_[p].SetAllUnmaskedHands(0.0);
    } else {
      logger::critical("unsupported range constructr mode %s", directive);
    }
  }

  void Copy(Ranges *that_range) const {
    for (int p = 0; p < num_player_; p++)
      beliefs_[p].CopyValue(&that_range->beliefs_[p]);
  }
  virtual ~Ranges() {
    delete[] beliefs_;
  }
  [[nodiscard]] double ValueSum() const {
    double v = 0.0;
    for (int p = 0; p < num_player_; p++)
      v += beliefs_[p].BeliefSum();
    return v;
  }
  int num_player_;
  sHandBelief *beliefs_ = nullptr;

  [[nodiscard]] bool ReturnReady(bool check_pruning) const {
    if (check_pruning) {
      //return if any side is all pruned. cuz it does not want to go down while it will be all zero even pruning is not on
      bool ready = beliefs_[0].AllPruned() || beliefs_[1].AllPruned();
      if (ready)
        return true;
    }
    //then check the all zero condition.
    return beliefs_[0].AllZero() && beliefs_[1].AllZero();
  }
};
#endif //BULLDOG_MODULES_ENGINE_SRC_HAND_BELIEF_H_
