//
// Created by Isaac Zhang on 5/21/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_TERM_EVAL_KERNEL_H_
#define BULLDOG_MODULES_ENGINE_SRC_TERM_EVAL_KERNEL_H_

#include "hand_belief.h"
static const int REGRET_SCALER = 1000; //turn into milli big blind. also counter the regret integer rounding problem

struct sHandAndRank {
  Card_t high_card;
  Card_t low_card;
  int rank;
  uint16_t v_idx;
  inline bool CheckCardCrash(Card_t check_card) {
    if (low_card == check_card) return true;
    return high_card == check_card;
  }
  inline std::array<Card_t, 2> GetHand() {
    return std::array<Card_t, 2>{high_card, low_card};
  }
  /*
   * for simple sorting by rank
   */
  inline bool RankLower(int that_rank) const {
    return rank < that_rank;
  }
  inline bool CardCrash(const sHandAndRank *that) {
    //gaurantee no crash
    if (high_card < that->low_card) return false;
    if (low_card > that->high_card) return false;
    //check crash
    if (CheckCardCrash(that->high_card)) return true;
    return CheckCardCrash(that->low_card);
  }
  inline bool RankEqual(const sHandAndRank *that) {
    return rank == that->rank;
  }
  /*
   * sort not just by hand but also by card
   * rank -> high -> low
   */
  inline bool RankHighLowSort(const sHandAndRank *that) const {
    if (rank < that->rank) return true;
    if (rank > that->rank) return false;
    // rank ==
    if (high_card > that->high_card) return true;
    if (high_card < that->high_card) return false;
    // rank == && high =+
    if (low_card > that->low_card) return true;
    if (low_card < that->low_card) return false;
    return false;
  }
  void Print() {
    logger::debug("high %d low %d | rank %d | idx = %d", high_card, low_card, rank, v_idx);
  }
};

/**
 * v0 - naive O(N2) eval | 30000mcs
 * v1 - with 52C2 hands | 17000 mcs
 * v2 - with 47C2 hands | 13000 mcs
 * v3 - cache card exclusion value | 4000 mcs
 * v4 - using integer iterator not stl iteration | 3200 mcs
 * v5 - cache rank index and net value | 1200 mcs
 * v6 - passing double matrix reference | 1160 mcs
 * v7 - cache exclusion index | 300 mcs
 * v8 - A vs B. cache A drift value | 45mcs
 * v9 - building exclusion in stack up, resursively 13 mcs.
 *
 *  future:
 *  - use all primitive type better, by size.
 *  - use sparse rank (accoridng to the unique rank by my and opp.
 */


class TermEvalKernel {
 public:
  std::array<sHandAndRank *, HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD> showdown_sorted_hand_ranks_;

  Board_t board_;
  int min_rank = 0;
  int unique_rank_count = 0;
  int *rank_first_equal_index_;  //rank starting
  int *rank_first_losing_index_; //next rank starting
  uint16_t high_low_pos_[52][52];
  //prepare related
  void Prepare(Board_t *board);
  inline void Sort();;
  void PreStack();
  //showdown
  void FastShowdownEval(double *opp_full_belief, double* my_full_belief, int spent);
  void NaiveShowdownEval(double *opp_belief, double* my_full_belief, int spent);
  void StackShowdownProb(double *opp_belief, double *rank_net_win_prob, double *card_rank_net, int *card_skipping_rank_list);
  //fold eval
  void FastFoldEval(double* opp_full_belief, double* my_full_belief, int spent);
  void NaiveFoldEval(double* opp_belief, double* my_belief, int spent);
  void StackFoldingProb(double* opp_belief, double* drift_by_card, double& sum);

  void FastTerminalEval(double *opp_belief, double* my_full_belief, int spent, bool showdown);

  virtual ~TermEvalKernel() {
    for (auto a : showdown_sorted_hand_ranks_) delete a;
    delete[] rank_first_losing_index_;
    delete[] rank_first_equal_index_;
  }
  static int ComboIdx(int rank, int card);
};

#endif //BULLDOG_MODULES_ENGINE_SRC_TERM_EVAL_KERNEL_H_
