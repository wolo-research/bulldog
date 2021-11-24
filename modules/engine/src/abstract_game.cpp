//
// Created by Isaac Zhang on 8/7/20.
//

#include "abstract_game.h"

void AbstractGame::IndexBettingTree(Node *this_node) {
  // if at the root node.
  if (this_node == nullptr) this_node = root_node_;
  if (this_node->IsTerminal())
    return;
  //index choice node only
  node_map_[this_node->GetRound()].insert(
      std::make_pair(this_node->GetActingPlayer(), this_node));
  for (auto c : this_node->children) {
    IndexBettingTree(c);
  }
}

/**
 * first filtered by round, and acting player
 *
 * then pick one out
 * @param real_state
 * @param strategy
 * @return
 */
void AbstractGame::MapToNode(State &real_state, NodeMatchCondition &condition) {
  if (node_map_.empty())
    IndexBettingTree();

  auto current_player = currentPlayer(&game_, &real_state);

  auto real_betting = GetBettingStr(&game_, real_state);
  auto range = node_map_[real_state.round].equal_range(current_player);

  NodeMatchCondition min_condition;
  min_condition.bet_sim_dist_ = 100;//a hack to make sure it has composite less than

  for (auto it = range.first; it != range.second; it++) {
    auto node = (*it).second;
    auto betting_similarity = SumBettingPatternDiff(&node->state_, &real_state);
    if (betting_similarity > 2)
      continue; //greedy skipping
//      auto node_betting_str = GetBettingStr(node->game_, node->state_);
//      auto betting_similarity = uiLevenshteinDistance(real_betting, node_betting_str);
    double L2_dist = PotL2(real_state, node->state_);
    int sum_bet_size_abs_diff = -1;
    if (betting_similarity == 0)
      sum_bet_size_abs_diff = DecayingBettingDistance(real_state, node->state_);

    auto new_condition = NodeMatchCondition{node, L2_dist, betting_similarity, sum_bet_size_abs_diff};
    if (new_condition < min_condition) {
      //and also the strategy is not uninitialized.
      min_condition.CopyValue(new_condition);
    }
  }
  //for extreme case where you have none matched nodes
  if (min_condition.bet_sim_dist_ == 100) {
    logger::warn("none node matched. terrible tree");
    min_condition.matched_node_ = range.first->second;
  }
  //set return
  condition.CopyValue(min_condition);
}
void AbstractGame::NormalizeRootReachProb() {
  Board_t board{};
  BoardFromState(&game_, &root_state_, &board);
  for (auto p = 0; p < GetActivePlayerNum(); p++) {
    root_hand_belief_[p].NormalizeExcludeBoard(board);

#if DEV > 1
    //check if all boards cards are pruned
    auto round = root_node_->GetRound();
    auto board_card = HoldemSumBoardMap[round];
    auto supposed_count = nCk_card(52, 2) - nCk_card(52 - board_card, 2);
    int count = 0;
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
      if (root_hand_belief_[p].IsPruned(i))
        count++;
    if (count != supposed_count)
      logger::critical("actual count != suposed count %d != %d", count, supposed_count);
#endif
  }
}

void AbstractGame::BuildKernelFromRootNode(Bucket_t *bucket_counts) {
  kernel_ = new sRNBAKernel{};
  if (root_node_ == nullptr)
    logger::critical("trying to build kernel from empty root node");
  kernel_->BuildInternal(root_node_, bucket_counts, depth_limited_);
}
void AbstractGame::PurifyLowProbRange() const {
  for (auto p = 0; p < GetActivePlayerNum(); p++)
    root_hand_belief_[p].Purify();
}
int AbstractGame::GetMaxRound() const {
  if (depth_limited_)
    return root_node_->GetRound();
  return HOLDEM_MAX_ROUNDS - 1;
}
