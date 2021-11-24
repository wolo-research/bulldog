//
// Created by Isaac Zhang on 2/25/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_STRATEGY_H_
#define BULLDOG_MODULES_ENGINE_SRC_STRATEGY_H_

#include "abstract_game.h"
#include "cfr_param.h"
#include "action_chooser.hpp"
#include "engine_util.h"

extern "C" {
#include <bulldog/game.h>
};

#include <string>

const int RANGE_ESTIMATE_SUCCESS = 111;
const int REAL_HAND_PRUNED_IN_TRANSITION = 112;
const int ALL_HERO_HAND_PRUNED_IN_TRANSITION = 113;
const int ALL_OPP_HAND_PRUNED_IN_TRANSITION = 114;

inline std::map<int, std::string> RangeEstimateCodeMap{
    {RANGE_ESTIMATE_SUCCESS, "RANGE_ESTIMATE_SUCCESS"},
    {REAL_HAND_PRUNED_IN_TRANSITION, "REAL_HAND_PRUNED_IN_TRANSITION"},
    {ALL_HERO_HAND_PRUNED_IN_TRANSITION, "ALL_HERO_HAND_PRUNED_IN_TRANSITION"},
    {ALL_OPP_HAND_PRUNED_IN_TRANSITION, "ALL_OPP_HAND_PRUNED_IN_TRANSITION"}
};


class Strategy {
 public:
  std::string name_ = "default";
  AbstractGame *ag_ = nullptr;
  ZIPAVG *zipavg_ = nullptr;
  //for cfr vecor
  DOUBLE_REGRET* double_regret_ = nullptr;
  ULONG_WAVG * ulong_wavg_ = nullptr;
  //for mccfr
  INT_REGRET * int_regret_ = nullptr;
  UINT_WAVG * uint_wavg_ = nullptr;
  std::ifstream* file_ptr = nullptr;
  //constructor and dest
  explicit Strategy(AbstractGame *ag) : ag_(ag) {
  }
  Strategy() = default;
  ~Strategy() {
    if (file_ptr != nullptr){
      file_ptr->close();
      delete file_ptr;
    }
    delete ag_;
    delete[] double_regret_;
    delete[] int_regret_;
    delete[] ulong_wavg_;
    delete[] uint_wavg_;
    delete[] zipavg_;
    logger::debug("strategy [%s] object gracefully shutting down", name_);
  }

  void SetAg(AbstractGame *ag) {
    ag_ = ag;
  }
  /*
   * compute strategy
   */
  int ComputeStrategy(Round_t r, Node_t n, Bucket_t b, Action_t a_max, float *rnb_avg, STRATEGY_TYPE mode) const;
  int ComputeStrategy(Node* node, Bucket_t b, float *rnb_avg, STRATEGY_TYPE mode) const;
  ZIPAVG GetZipAvg(RNBA idx) const;

  /*
   * strategy allocation
   */
  void InitMemoryAndValue(CFR_MODE cfr_mode = CFR_UNKNOWN);
  void AllocateMemory(STRATEGY_TYPE type, CFR_MODE cfr_mode = CFR_UNKNOWN);
  void InitMemory(STRATEGY_TYPE type, CFR_MODE mode);
  void DiscountStrategy(STRATEGY_TYPE type, double factor) const;

  /*
   * for the compressed strategy only
   */
  void ClearZipAvgMemory();
  void ConvertWavgToZipAvg(pthread_t *thread_pool, unsigned int num_threads) const;

  /*
   * act and pick function
   */
  bool EstimateNewAgReach(AbstractGame *new_ag, MatchState *new_match_state, STRATEGY_TYPE type) const;
  int EstimateReachProbAtNode(MatchState *last_match_state,
                              Node *target_node,
                              std::array<sHandBelief, 2> &new_ag_reach,
                              STRATEGY_TYPE calc_mode,
                              double min_filter = DEFAULT_BAYESIAN_TRANSITION_FILTER) const;
  void PickAction(MatchState *match_state,
                  ActionChooser *action_chooser,
                  Action &r_action,
                  STRATEGY_TYPE calc_mode,
                  Node *matched_node);
  Bucket_t GetBucketFromMatchState(MatchState *match_state) const;

  /*
   * inspection.
   */
  void InspectNode(Node *inspect_node, const std::string &prefix, STRATEGY_TYPE calc_mode);
  void PrintNodeStrategy(Node *node, Bucket_t b, STRATEGY_TYPE calc_mode) const;
  void InspectStrategyByMatchState(MatchState *match_state, STRATEGY_TYPE calc_mode);
  void InspectPreflopBets(const std::string& print_name, STRATEGY_TYPE calc_mode);
  bool FreezePriorAction(Strategy *old_strategy, MatchState *real_match_state) const;
  void CheckFrozenStrategyConsistency(Strategy *old_strategy, MatchState *new_match_state) const;
  static void *ThreadedZipAvgConvert(void *thread_args);
  bool IsStrategyInitializedForMyHand(Node *matched_node, STRATEGY_TYPE strategy_type, MatchState *match_state) const;
  std::vector<NodeMatchCondition>
  FindSortedMatchedNodes(State &state) const;
};

struct sThreadInputZipavgConvert{
  sThreadInputZipavgConvert(const Strategy *strategy, Bucket_t b_begin, Bucket_t b_end, int round)
      : strategy(strategy), b_begin(b_begin), b_end(b_end), round(round) {}
  const Strategy* strategy;
  Bucket_t b_begin;
  Bucket_t b_end;
  int round;
};

#endif //BULLDOG_MODULES_ENGINE_SRC_STRATEGY_H_
