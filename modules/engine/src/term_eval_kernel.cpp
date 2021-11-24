//
// Created by Isaac Zhang on 5/21/20.
//

#include "term_eval_kernel.h"
void TermEvalKernel::Prepare(Board_t *board) {
  board_ = *board;
  int index = 0;
  std::set<int> rank_set;
  for (Card_t low = 0; low < HOLDEM_MAX_CARDS - 1; low++) {
    for (Card_t high = low + 1; high < HOLDEM_MAX_CARDS; high++) {
      auto hand = Hand_t{high, low};
      if (board->HandCrash(hand)) continue;
      int rank = RankHand(high, low, board);
      auto v_idx = ToVectorIndex(high, low);
      showdown_sorted_hand_ranks_[index] = new sHandAndRank{high, low, rank, v_idx};
      index++;
      rank_set.insert(rank);
    }
  }

  if (index != nCk_card(47, 2)) logger::error("total number of hands not right = %d", index);
  unique_rank_count = rank_set.size();
  //allocate memory
  rank_first_equal_index_ = new int[unique_rank_count];
  rank_first_losing_index_ = new int[unique_rank_count];

  Sort();
  PreStack();
}
void TermEvalKernel::Sort() {
  std::sort(showdown_sorted_hand_ranks_.begin(), showdown_sorted_hand_ranks_.end(),
            [](const sHandAndRank *lhs, const sHandAndRank *rhs) {
              return lhs->RankHighLowSort(rhs);
            });
//  for (auto a : showdown_sorted_hand_ranks_)
//    a->Print();

  min_rank = showdown_sorted_hand_ranks_[0]->rank;
  for (unsigned long i = 0; i < showdown_sorted_hand_ranks_.size(); i++) {
    auto h = showdown_sorted_hand_ranks_[i]->high_card;
    auto l = showdown_sorted_hand_ranks_[i]->low_card;
    high_low_pos_[h][l] = i;

//    Cardset c = emptyCardset();
//    addCardToCardset(&c, suitOfCard(list_[i]->high_card, 4), rankOfCard(list_[i]->high_card, 4));
//    addCardToCardset(&c, suitOfCard(list_[i]->low_card, 4), rankOfCard(list_[i]->low_card, 4));
//    printf("%s pos:%d\n", CardsToString(c.cards).c_str(), i);
  }
}
void TermEvalKernel::PreStack() {
  int last_seen_rank = min_rank;
  int rank_index = 0;
  rank_first_equal_index_[rank_index] = 0;

  for (int i = 0; i < HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD; i++) {
    auto l = showdown_sorted_hand_ranks_[i];
    auto rank = l->rank;

    if (rank != last_seen_rank) {
      last_seen_rank = rank;
      rank_index++;
      //new rank. store its value and index
      rank_first_equal_index_[rank_index] = i;
    }
  }
  for (int j = 0; j < unique_rank_count - 1; j++) {
    rank_first_losing_index_[j] = rank_first_equal_index_[j + 1];
  }
  rank_first_losing_index_[unique_rank_count - 1] = HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD;
}
/**
 * it is board-safe
 * @param opp_belief
 * @param A
 * @param spent
 */
void TermEvalKernel::FastShowdownEval(double *opp_full_belief,
                                      double *my_full_belief,
                                      int spent) {
  //transform opp belief from 1326 into 1081 accordingly
  double opp_belief[HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD];
  for (auto i = 0; i < HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD; i++) {
    opp_belief[i] = opp_full_belief[showdown_sorted_hand_ranks_[i]->v_idx];
  }

  double rank_net_win[unique_rank_count];
  double card_net[HOLDEM_MAX_CARDS * HOLDEM_MAX_CARDS];
  for (auto &i : card_net) i = 0;
  int card_skipping_rank[46 * HOLDEM_MAX_CARDS];//52-5-1
  for (auto &i :card_skipping_rank)
    i = -1;  //-1 as a cutoff flag

  StackShowdownProb(opp_belief, rank_net_win, card_net, card_skipping_rank);

  int card_last_skip_idx[HOLDEM_MAX_CARDS];
  for (auto &i : card_last_skip_idx)
    i = 0;

  //for computing the drift
  double card_last_net[HOLDEM_MAX_CARDS];
  for (auto c = 0; c < HOLDEM_MAX_CARDS; c++)
    card_last_net[c] = card_net[ComboIdx(0, c)];

  for (int rank_i = 0; rank_i < unique_rank_count; rank_i++) {
    double base = rank_net_win[rank_i];
    for (int j = rank_first_equal_index_[rank_i]; j < rank_first_losing_index_[rank_i]; j++) {
      auto v_idx = showdown_sorted_hand_ranks_[j]->v_idx;
      //pruning
      if (my_full_belief[v_idx] == BELIEF_MASK_VALUE)
        continue;

      double total_drift = 0.0;
      for (auto &c :showdown_sorted_hand_ranks_[j]->GetHand()) {
        auto idx = ComboIdx(card_last_skip_idx[c], c);
        if (card_skipping_rank[idx] != rank_i) {
          card_last_skip_idx[c]++;
          card_last_net[c] = card_net[ComboIdx(card_last_skip_idx[c], c)];
        }
        total_drift += card_last_net[c];  // no need to delete double count [high, low] as it must be the same value
      }
      double net = base - total_drift;
      my_full_belief[v_idx] = net * spent;
    }
  }
}

void TermEvalKernel::NaiveShowdownEval(double *opp_belief,
                                       double *my_full_belief,
                                       int spent) {
  auto begin = std::chrono::steady_clock::now();
  for (auto my_pos = 0; my_pos < HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD; my_pos++) {
    auto v_idx = showdown_sorted_hand_ranks_[my_pos]->v_idx;
    //pruning
    if (my_full_belief[v_idx] == BELIEF_MASK_VALUE)
      continue;
    auto weighted_sum = 0.0;

    for (auto opp_pos = 0; opp_pos < HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD; opp_pos++) {
      if (my_pos == opp_pos)
        continue;
      auto weight = opp_belief[showdown_sorted_hand_ranks_[opp_pos]->v_idx];
      //no hand belief or just too low
      if (weight == 0)
        continue;
      if (showdown_sorted_hand_ranks_[my_pos]->RankEqual(showdown_sorted_hand_ranks_[opp_pos]))
        continue;
      if (showdown_sorted_hand_ranks_[my_pos]->CardCrash(showdown_sorted_hand_ranks_[opp_pos]))
        continue;
      //compare
      weighted_sum += my_pos > opp_pos ? weight : -1.0 * weight;
    }
    my_full_belief[v_idx] = weighted_sum * spent;
  }
  auto end = std::chrono::steady_clock::now();
  auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
  logger::trace("naive showdown || term node eval takes = %d  (microseconds)", total_time_ms);

}

/**
 * not board-card safe
 * @param opp_full_belief
 * @param my_full_belief
 * @param spent
 * @param win_loss_multiplier win = 1, lose = -1
 */
void TermEvalKernel::FastFoldEval(double *opp_full_belief,
                                  double *my_full_belief,
                                  int spent) {
  double folding_drift[HOLDEM_MAX_CARDS];
  for (auto &i :folding_drift) i = 0;
  double base = 0.0;
  StackFoldingProb(opp_full_belief, folding_drift, base);
  for (auto my_pos = 0; my_pos < HOLDEM_MAX_HANDS_PERMUTATION_EXCLUDE_BOARD; my_pos++) {
    auto v_idx = showdown_sorted_hand_ranks_[my_pos]->v_idx;
    //pruning
    if (my_full_belief[v_idx] == BELIEF_MASK_VALUE)
      continue;
    auto high_low = FromVectorIndex(v_idx);
    auto total_drift =
        folding_drift[high_low.first]
            + folding_drift[high_low.second] - opp_full_belief[v_idx]; // - double count
    auto net = base - total_drift;
    my_full_belief[v_idx] = net * spent;
  }
}

void TermEvalKernel::NaiveFoldEval(double *opp_belief,
                                   double *my_belief,
                                   int spent) {
  auto begin = std::chrono::steady_clock::now();
  for (auto my_pos = 0; my_pos < FULL_HAND_BELIEF_SIZE; my_pos++) {
    //pruning
    if (my_belief[my_pos] == BELIEF_MASK_VALUE)
      continue;
    auto weighted_sum = 0.0;

    for (auto opp_pos = 0; opp_pos < FULL_HAND_BELIEF_SIZE; opp_pos++) {
      if (my_pos == opp_pos)
        continue;
      //check if i and j clashes
      if (VectorIdxClash(my_pos, opp_pos))
        continue;
      auto weight = opp_belief[opp_pos];
      //no hand belief or just too low
      if (weight > 0.0){
        weighted_sum += weight;
      }
    }
    my_belief[my_pos] = weighted_sum * spent;
  }
  auto end = std::chrono::steady_clock::now();
  auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
  logger::trace("naive fold || term node eval takes = %d  (microseconds)", total_time_ms);

}

void TermEvalKernel::StackFoldingProb(double *opp_belief,
                                      double *drift_by_card, double &sum) {
  for (int i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
    double w = opp_belief[i];
    //skipping 0 and -1
    if (w > 0.0) {
      sum += w;
      auto high_low = FromVectorIndex(i);
      drift_by_card[high_low.first] += w;
      drift_by_card[high_low.second] += w;
    }
  }
}

/**
 * the net sum is calculated iteratively...
 * using a skipping linked list.
 * @param opp_belief
 * @param rank_net_win_prob
 * @param card_rank_net
 * @param card_skipping_rank_list , default -1
 */
void TermEvalKernel::StackShowdownProb(double *opp_belief,
                                       double *rank_net_win_prob,
                                       double *card_rank_net,
                                       int *card_skipping_rank_list) {
  double rank_sum[unique_rank_count];
  for (auto &i : rank_sum) i = 0;

  //temporary object for constructing card_skipping_rank_list
  //e.g. 0->4, 1->67, 2->89, 3->126... skip_list_idx -> rank
  int card_last_skipping_list_dx[HOLDEM_MAX_CARDS];
  for (auto &i : card_last_skipping_list_dx)
    i = -1;

  //skipping linked list no more than 52 - 5 - 1 = 46 unique rank.  rank * card
  // in practise it is a lot less
  double card_rank_sum[46][HOLDEM_MAX_CARDS];
  for (auto &i : card_rank_sum)
    for (double &j : i)
      j = 0;

  double card_sum[HOLDEM_MAX_CARDS];
  for (auto &i :card_sum)
    i = 0;

  //first list iteration, tally the sum of belief, by rank, by card
  double opp_belief_sum = 0.0;
  for (int rank_i = 0; rank_i < unique_rank_count; rank_i++) {
    for (int j = rank_first_equal_index_[rank_i]; j < rank_first_losing_index_[rank_i]; j++) {
      double w = opp_belief[j];
      //WARNING: the below code snippet makes showdown not able to handle 0 items in opp belief! maybe because it makes the skipping rank_list part not accurate, i.e. missing some steps
//      if (w == 0)
//        continue;
      rank_sum[rank_i] += w;

      //card
      for (auto &c : showdown_sorted_hand_ranks_[j]->GetHand()) {
        int idx = ComboIdx(card_last_skipping_list_dx[c], c);
        //if idx < 0. then it is not init at all, ++ to 0
        if (idx < 0 || card_skipping_rank_list[idx] != rank_i) {
          card_last_skipping_list_dx[c]++;
          //present this one to rank_i, equivalent to ComboIdx(card_last_skipping_list_dx[c], c);, after ++
          card_skipping_rank_list[idx + HOLDEM_MAX_CARDS] = rank_i;
        }
        card_rank_sum[card_last_skipping_list_dx[c]][c] += w;
        card_sum[c] += w;
      }
    }
    opp_belief_sum += rank_sum[rank_i];
  }

  //second iters, iteratively compute needed values by rank

  //computing the base  //add the rank-1 and rank local sum
  for (int rank_i = 0; rank_i < unique_rank_count; rank_i++) {
    if (rank_i == 0) {
      rank_net_win_prob[rank_i] = rank_sum[rank_i] - opp_belief_sum;
      continue;
    }
    rank_net_win_prob[rank_i] = rank_net_win_prob[rank_i - 1] + rank_sum[rank_i - 1] + rank_sum[rank_i];
  }

  //compute the card drift, skipping linked list.
  for (auto c = 0; c < HOLDEM_MAX_CARDS; c++) {
    //skipping non-legit card
    if (board_.CardCrash(c))
      continue;

    int skip_idx = 0;
    while (true) {
      if (skip_idx == 0) {
        card_rank_net[ComboIdx(skip_idx, c)] =
            card_rank_sum[skip_idx][c] - card_sum[c];
        skip_idx++;
        continue;
      }
      int combo_idx = ComboIdx(skip_idx, c);
      //cut off if -1
      if (card_skipping_rank_list[combo_idx] == -1)
        break;
      card_rank_net[combo_idx] =
          card_rank_net[combo_idx - HOLDEM_MAX_CARDS] + card_rank_sum[skip_idx][c]
              + card_rank_sum[skip_idx - 1][c];  //card_net[combo_idx - HOLDEM_MAX_CARDS]  equals combo(skip_idx-1, c)
      skip_idx++;
    }
  }
}

inline int TermEvalKernel::ComboIdx(int rank, int card) {
  return rank * HOLDEM_MAX_CARDS + card;
}

void TermEvalKernel::FastTerminalEval(double *opp_belief, double *my_full_belief, int spent, bool showdown) {
  if (showdown){
    FastShowdownEval(opp_belief, my_full_belief, spent);
  } else {
    FastFoldEval(opp_belief, my_full_belief, spent);
  }
}
