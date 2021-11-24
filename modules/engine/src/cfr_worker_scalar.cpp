//
// Created by Carmen C on 16/6/2020.
//

#include "cfr_worker.h"

double ScalarCfrWorker::Solve(Board_t board) {
  auto ag = strategy_->ag_;
  //sampling board

  auto active_players = AbstractGame::GetActivePlayerNum();
  auto hand_info = HandInfo(active_players, board, gen);
  //generate root belief based on the board
  std::array<sHandBelief *, 2> local_root_belief{};
  for (int p = 0; p < active_players; p++) {
    //prepare
    local_root_belief[p] = new sHandBelief(&ag->root_hand_belief_[p]);
    local_root_belief[p]->NormalizeExcludeBoard(board);
  }

  int REPS = 1000;
  double util = 0.0;
  for (int loc_iter = 0; loc_iter < REPS; loc_iter++) {
    hand_info.Sample(ag, local_root_belief);
    if (cfr_param_->pruning_on && cfr_param_->rm_floor < 0)
      iter_prune_flag = GenRndNumber(1, 100) <= cfr_param_->rollout_prune_prob * 100;

    for (int trainee_pos = 0; trainee_pos < active_players; trainee_pos++)
      util += WalkTree(trainee_pos, ag->root_node_, hand_info);
  }

  //do a side walk for updating wavg
  if (cfr_param_->avg_side_update_ && cfr_param_->rm_avg_update == AVG_CLASSIC) {
    int SIDE_REPS = 50;
    for (int loc_iter = 0; loc_iter < SIDE_REPS; loc_iter++) {
      hand_info.Sample(ag, local_root_belief);
      for (int trainee_pos = 0; trainee_pos < active_players; trainee_pos++)
        WavgUpdateSideWalk(trainee_pos, ag->root_node_, hand_info);
    }
  }
  util /= (REPS * 2 * ag->GetBigBlind());

  for (auto hb : local_root_belief)
    delete hb;
  return util;
}

double ScalarCfrWorker::WalkTree(int trainee_pos, Node *this_node, HandInfo &hand_info) {
  if (this_node->IsTerminal()) {
    return EvalTermNode(trainee_pos, this_node, hand_info);
  }
  //depth limited solving, till the inital root of next round (not initial root of this round)
  if (this_node->IsLeafNode()) {
    return EvalRootLeafNode(trainee_pos, this_node, hand_info);
  }
  return EvalChoiceNode(trainee_pos, this_node, hand_info);
}

double ScalarCfrWorker::EvalChoiceNode(int trainee_pos, Node *this_node, HandInfo &hand_info) {
  int acting_player = this_node->GetActingPlayer();
  bool is_my_turn = acting_player == trainee_pos;
  auto a_max = this_node->GetAmax();
  auto r = this_node->GetRound();
  auto n = this_node->GetN();
  auto b = hand_info.buckets_[acting_player][r];
  auto rnb0 = strategy_->ag_->kernel_->hash_rnba(r, n, b, 0);

  if (is_my_turn) {
    double child_cfu[a_max];
    bool prune_flag[a_max];
    for (auto a = 0; a < a_max; a++) {
      //do pruning if on. skip river nodes and skip nodes leading to terminal
      prune_flag[a] = false;
      auto next_node = this_node->children[a];
      if (iter_prune_flag && !next_node->IsTerminal() && next_node->GetRound() != HOLDEM_ROUND_RIVER) {
//      if (iter_prune_flag && !next_node->IsTerminal()) {
        if (strategy_->int_regret_[rnb0 + a] <= cfr_param_->rollout_prune_thres) {
          prune_flag[a] = true;
          continue;
        }
      }
      child_cfu[a] = WalkTree(trainee_pos, next_node, hand_info);
    }

    //only supported weighted response. check outside
    if (cfr_param_->cfu_compute_acting_playing != WEIGHTED_RESPONSE)
      logger::critical("scalar does not support best response eval");

    double cfu = 0.0;
    float avg[a_max];
    strategy_->ComputeStrategy(this_node, b, avg, cfr_param_->strategy_cal_mode_);
    //compute cfu
    for (auto a = 0; a < a_max; a++) {
      if (prune_flag[a])
        continue;
      cfu += avg[a] * child_cfu[a];
    }

    //only do it in weighted response.
    for (auto a = 0; a < a_max; a++) {
      if (prune_flag[a])
        continue;
      int diff = (int) round(child_cfu[a] - cfu);
      double temp_reg = strategy_->int_regret_[rnb0 + a] + diff;
      //clamp it
      int new_reg = (int) std::fmax(temp_reg, cfr_param_->rm_floor);
      //total regret should have a ceiling
      if (new_reg > 2107483647) {
        logger::critical(
            "regret if overflowing. think the regret %d is not converging. [temp_reg %f][diff %f][cfu_a %f][cfu %f]",
            new_reg,
            temp_reg,
            diff,
            child_cfu[a],
            cfu);
      }//2147483647 - 4 * 10 ^ 7{
      strategy_->int_regret_[rnb0 + a] = new_reg;
    }

    return cfu;
  }

  // opp turn, do external sampling.
  float rnb_avg[a_max];
  strategy_->ComputeStrategy(this_node, b, rnb_avg, cfr_param_->strategy_cal_mode_);
  int sampled_a = RndXorShift<float>(rnb_avg, a_max, x, y, z, (1 << 16));
  if (sampled_a == -1) {
    strategy_->PrintNodeStrategy(this_node, b, cfr_param_->strategy_cal_mode_);
    logger::critical("new strategy regret problem in main walk, opp side");
  }

  if (!cfr_param_->avg_side_update_ && cfr_param_->rm_avg_update == AVG_CLASSIC) {
    //update wavg. dont use it for blueprint training as it explodes quickly
//    for (auto a = 0; a < a_max; a++) {
//      strategy_->uint_wavg_[rnb0 + a] += rnb_avg[a] * 1000;
//    }
    strategy_->uint_wavg_[rnb0 + sampled_a] += 1;
  }
  return WalkTree(trainee_pos, this_node->children[sampled_a], hand_info);
}

//pluribus'way to update wavg.
void ScalarCfrWorker::WavgUpdateSideWalk(int trainee_pos, Node *this_node, HandInfo &hand_info) {
  if (this_node->IsTerminal())
    return;
  if (this_node->IsLeafNode())
    return;

  bool is_my_turn = this_node->GetActingPlayer() == trainee_pos;
  auto a_max = this_node->GetAmax();

  if (is_my_turn) {
    int r = this_node->GetRound();
    auto n = this_node->GetN();
    Bucket_t b = hand_info.buckets_[trainee_pos][r];
    float rnb_avg[a_max];
    strategy_->ComputeStrategy(this_node, b, rnb_avg, cfr_param_->strategy_cal_mode_);
    int sampled_a = RndXorShift<float>(rnb_avg, a_max, x, y, z, (1 << 16));
    if (sampled_a == -1) {
      logger::debug("new strategy regret problem");
      strategy_->PrintNodeStrategy(this_node, b, cfr_param_->strategy_cal_mode_);
    }
    auto rnba = strategy_->ag_->kernel_->hash_rnba(r, n, b, sampled_a);
    strategy_->uint_wavg_[rnba] += 1;
    WavgUpdateSideWalk(trainee_pos, this_node->children[sampled_a], hand_info);
  } else {
    //opp turn
    for (auto a = 0; a < a_max; a++) {
      WavgUpdateSideWalk(trainee_pos, this_node->children[a], hand_info);
    }
  }
}
double ScalarCfrWorker::EvalTermNode(int trainee_pos, Node *this_node, HandInfo &hand_info) {
  if (this_node->IsShowdown()) {
    int stake = this_node->GetStake(trainee_pos);
    stake *= hand_info.payoff_[trainee_pos];//tie 0, win 1, lose -1
    return (double) stake;
  } else {
    //fold
    int stake = this_node->GetStake(trainee_pos);
    return (double) stake;
  }
}

/*
 * v0: lazy rollout for traverser, 3 reps avg, both using non-biased strategy
 * v1: lazy rollout, 3 reps, alternating, external sampling, avg cfu.
 * v2: profile it before you cache anything. cache cfu for the leaf node (by node ID and hands), and optimize leaf node action caching
 * v3: cache preflop as file
 * v4: ...
 */
double ScalarCfrWorker::EvalRootLeafNode(int trainee_pos, Node *this_node, HandInfo &hand_info) {
  auto r = this_node->GetRound() - 1; //should be the last round.
  /*
   * read from cache, if exist
   * (PREFLOP) ram cache from file
   * (FLOP) rollout/NN cache,  if needed
   */
  auto b0 = 0;
  auto b1 = 0;
  if (cfr_param_->depth_limited_cache_) {
    this_node->CreateValueCacheIfNull();
    if (r > HOLDEM_ROUND_FLOP) {
      logger::critical("not doing dls post flop");
    }
    b0 = hand_info.buckets_[0][r];
    b1 = hand_info.buckets_[1][r];
    if (this_node->value_cache_->Exists(b0, b1)) {
      auto p0_cfu = this_node->value_cache_->GetValue(b0, b1);
      if (trainee_pos == 1)
        p0_cfu *= -1;
      return p0_cfu;
    }
  }

  /*
   * ROLLOUT
   */
  double cfu_0 = LeafRootRollout(trainee_pos, this_node, hand_info);

  /*
   * or use NN
   */

  //insert to cache
  if (cfr_param_->depth_limited_cache_)
    this_node->value_cache_->SetValue(b0, b1, cfu_0);
  return trainee_pos == 0 ? cfu_0 : -cfu_0;
}

double ScalarCfrWorker::WalkLeafTree(int trainee_pos,
                                     Node *this_node,
                                     HandInfo &hand_info,
                                     int *c_strategy) {
#if 0
  this_node->PrintState("leaf node: ");
#endif
  if (this_node->IsTerminal()) {
    return EvalTermNode(trainee_pos, this_node, hand_info);
  }
  return LeafChoiceRollout(trainee_pos, this_node, hand_info, c_strategy);
}

double ScalarCfrWorker::LeafRootRollout(int trainee_pos, Node *this_node, HandInfo &hand_info) {
  //map this_node to the node of blueprint
  NodeMatchCondition condition;
  //todo: change the strategy match node
  blueprint_->ag_->MapToNode(this_node->state_, condition);
  auto matched_node = condition.matched_node_;

  //do multiple rollout starting from the matched node till u hit terminal
  HandInfo subgamg_hand_info(hand_info.num_players, hand_info.board_, gen);
  subgamg_hand_info.hand_[0] = hand_info.hand_[0];
  subgamg_hand_info.hand_[1] = hand_info.hand_[1];
  //fill the real board according to the round you are in.
  auto r = this_node->GetRound();
  int sum_bc = r == HOLDEM_ROUND_PREFLOP ? 0 : 3;
  for (int c = sum_bc; c < HOLDEM_MAX_BOARD; c++)
    subgamg_hand_info.board_.cards[c] = IMPOSSIBLE_CARD;
  int rollout_rep = cfr_param_->depth_limited_rollout_reps_;
  /*
   * V1, External sampling
   */

  //allocate regret for each strategy, four strategy. the regret should be global?
  double c_str_regret[2][MAX_META_STRATEGY];
  for (auto &a : c_str_regret)
    for (auto &b : a)
      b = 0;

  double final_cfu[2];

  /*
   * reps 3
   *  traverser 2
   *    strategy. 4
   *
   *    doing 24 probing
   */
  for (int i = 0; i < rollout_rep; i++) {

    /*
     * sample the board, not crashing with the hand todo: can make it a testable function
     */
    HoldemDeck deck{subgamg_hand_info.board_};
    deck.Shuffle();
    int total_bc = sum_bc;
    int deck_cursor = 0;
    while (total_bc <= HOLDEM_MAX_BOARD) {
      auto sample_card = deck.cards_[deck_cursor++];
      if (VectorIdxClashCard(subgamg_hand_info.hand_[0], sample_card)
          || VectorIdxClashCard(subgamg_hand_info.hand_[1], sample_card)) {
        continue;
      }
      subgamg_hand_info.board_.cards[total_bc++] = sample_card;
    }
//    subgamg_hand_info.board_.Print();
    subgamg_hand_info.SetBucketAndPayoff(blueprint_->ag_);

    for (int p = 0; p < 2; p++) {
      //pick a strategy for opp
      float opp_avg[MAX_META_STRATEGY];
      GetPolicy<double>(opp_avg, MAX_META_STRATEGY, c_str_regret[1 - p]);
      auto opp_choice = RndXorShift<float>(opp_avg, MAX_META_STRATEGY, x, y, z, (1 << 16));
      if (opp_choice == -1) {
        logger::debug("depth limit meta strategy regret problem");
        for (int g = 0; g < MAX_META_STRATEGY; g++)
          logger::debug("%f", c_str_regret[1 - p][g]);
      }
      double cfu_s[MAX_META_STRATEGY];

      //for each strategy of acting player
      for (int s = 0; s < MAX_META_STRATEGY; s++) {
        int c_strategy[2];
        c_strategy[p] = s;
        c_strategy[1 - p] = opp_choice;
        //rollout with the strategy.
        cfu_s[s] = WalkLeafTree(p, matched_node, subgamg_hand_info, c_strategy);
      }

      float avg[4]{};
      GetPolicy<double>(avg, MAX_META_STRATEGY, c_str_regret[p]);
      double cfu = 0.0;
      for (int s = 0; s < MAX_META_STRATEGY; s++) {
        cfu += avg[s] * cfu_s[s];
      }

      //if at the final iter
      if (i == rollout_rep - 1) {
        final_cfu[p] = cfu;
      } else {
        //update regret
        for (int s = 0; s < MAX_META_STRATEGY; s++) {
          double diff = cfu_s[s] - cfu;
          c_str_regret[p][s] += diff;
        }
      }
    }
  }
  //only player 0
  return final_cfu[0];
}

double ScalarCfrWorker::LeafChoiceRollout(int trainee_pos,
                                          Node *this_node,
                                          HandInfo &hand_info,
                                          int *p_meta) {
  auto r = this_node->GetRound();
  auto acting_player = this_node->GetActingPlayer();
  auto b = hand_info.buckets_[acting_player][r];
  int a_max = this_node->GetAmax();
  float avg[a_max];
  //didnot impl the get zipavg from node.
  blueprint_->ComputeStrategy(this_node, b, avg, STRATEGY_ZIPAVG); //by default blueprint only use zipavg

  //adjust accoridng to the biased strategy
  auto continuation_strategy = p_meta[acting_player];
  //find out the children that called
  int calling_idx = -1;
  for (auto c : this_node->children) {
    if (c->GetLastAction().type == a_call) {
      calling_idx = c->sibling_idx_;
      break;
    }
  }
  if (calling_idx == -1) {
    this_node->PrintAllChildState();
    logger::critical("calling should always be an available action");
  }

  switch (continuation_strategy) {
    case BIASED_CALLING:avg[calling_idx] *= BIASED_SCALER;
      break;
    case BIASED_RAISING:
      for (int i = calling_idx + 1; i < a_max; i++)
        avg[i] *= BIASED_SCALER;
      break;
    case BIASED_FOLDING:
      if (calling_idx == 1)
        avg[0] *= BIASED_SCALER;
      break;
    case BIASED_NONE:
      //do nth
      break;
  }

  //renomalize avg
  float sum_local_avg = 0.0;
  for (auto a = 0; a < a_max; a++)
    sum_local_avg += avg[a];
  auto scaler = 1.0 / sum_local_avg;
  for (auto a = 0; a < a_max; a++)
    avg[a] *= scaler;

  //pick.
  int sampled_a = RndXorShift<float>(avg, a_max, x, y, z, (1 << 16));
  if (sampled_a == -1) {
    logger::debug("blueprint zip problem");
    blueprint_->PrintNodeStrategy(this_node, b, STRATEGY_ZIPAVG);
    exit(1);
  }

#if 0
  logger::debug("select child leaf node %d", this_node->children[sampled_a]->GetLastActionCode());
#endif
  double cfu = WalkLeafTree(trainee_pos, this_node->children[sampled_a], hand_info, p_meta);
  return cfu;
}
