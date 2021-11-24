//
// Created by Isaac Zhang on 2/25/20.
//
#include <bulldog/game_util.h>
#include "strategy.h"

void Strategy::InitMemory(STRATEGY_TYPE type, CFR_MODE mode) {
  auto size = ag_->kernel_->MaxIndex();
  AllocateMemory(type, mode);
  switch (type) {
    case STRATEGY_REG:
      if (double_regret_ != nullptr) {
        for (RNBA i = 0; i < size; i++)
          double_regret_[i] = 0.0;
      } else {
        for (RNBA i = 0; i < size; i++)
          int_regret_[i] = 0;
      }
      break;
    case STRATEGY_WAVG:
      if (ulong_wavg_ != nullptr) {
        for (RNBA i = 0; i < size; i++)
          ulong_wavg_[i] = 0;
      } else {
        for (RNBA i = 0; i < size; i++)
          uint_wavg_[i] = 0;
      }
      break;
    case STRATEGY_ZIPAVG:
      for (RNBA i = 0; i < size; i++)
        zipavg_[i] = 0;
      break;
    default:
      logger::critical("unsupported strategy type %s", StrategyToNameMap[type]);
  }
  logger::trace("%d init to 0 | size = %d", type, size);
}

void Strategy::InitMemoryAndValue(CFR_MODE cfr_mode) {
  InitMemory(STRATEGY_REG, cfr_mode);
  InitMemory(STRATEGY_WAVG, cfr_mode);
}

/*
 * - first filter board, hand crash
 * - check if hero reach is already 0.
 */
bool Strategy::EstimateNewAgReach(AbstractGame *new_ag, MatchState *new_match_state, STRATEGY_TYPE type) const {
  std::array<sHandBelief, 2> new_base_reach;
  new_base_reach[0].CopyValue(&ag_->root_hand_belief_[0]);
  new_base_reach[1].CopyValue(&ag_->root_hand_belief_[1]);

  if (StateBettingEqual(&ag_->root_node_->state_, &new_ag->root_state_)) {
    logger::debug("the new strategy having the same root node. it happens when step back to last root");
    new_ag->root_hand_belief_[0].CopyValue(&new_base_reach[0]);
    new_ag->root_hand_belief_[1].CopyValue(&new_base_reach[1]);
    new_ag->NormalizeRootReachProb();
    return true;
  }
  auto new_round = new_match_state->state.round;
#if DEV > 1
  /*
   * safe guarding | the root belief of THIS_AG is correct.
   */
  Board_t this_board{};
  BoardFromState(&ag_->game_, &ag_->root_state_, &this_board);
  for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
    auto high_low = FromVectorIndex(i);
    if (this_board.CardCrashTillRound(high_low.first, new_round)
        || this_board.CardCrashTillRound(high_low.second, new_round)) {
      for (auto p = 0; p < AbstractGame::GetActivePlayerNum(); p++) {
        if (!ag_->root_hand_belief_[p].IsPruned(i))
          logger::warn(
              "root reach prob estimate failed.. beliefs of player [%d] at board cards [round %d] is not pruned [%f]",
              p,
              new_round,
              ag_->root_hand_belief_[p].belief_[i]);
      }
    }
  }
#endif

  //refine opponent reach that crashes with my hand.
  Board_t reach_board{};
  BoardFromState(&ag_->game_, &new_match_state->state, &reach_board);
  int hero_pos = new_match_state->viewingPlayer;
  int opp_pos = 1 - hero_pos;
  auto my_hand_vector_idx = ToVectorIndex(new_match_state->state.holeCards[hero_pos][0],
                                          new_match_state->state.holeCards[hero_pos][1]);
  new_base_reach[opp_pos].CleanCrashHands(my_hand_vector_idx);

  //this should never happen, as we will check the reach of newly solve strategy.
  if (new_base_reach[hero_pos].IsZero(my_hand_vector_idx)) {
    logger::warn("[player %d] [hand idx %d] [%s] [%.13f] == 0 before doing the transition. should look into this",
                 hero_pos,
                 my_hand_vector_idx,
                 VectorIdxToString(my_hand_vector_idx),
                 new_base_reach[hero_pos].belief_[my_hand_vector_idx]);
  }

  auto candidate_match = FindSortedMatchedNodes(new_ag->root_state_);
  NodeMatchCondition last_condition;
  int attempt_cursor = 1;
  for (auto match : candidate_match) {
    logger::require_warn(last_condition < match, "should be sorted man!!");

    //if it is my node and it is not the begining of the round, should check if i could have a good estimate
    //cuz the new board card may have not been sampled at all, when using subgame solving.
    auto kth_action = new_ag->root_state_.numActions[new_round];
    bool should_check_initialized = new_match_state->viewingPlayer == new_ag->root_node_->GetActingPlayer() &&
                                    kth_action > 0;
    if (should_check_initialized) {
      //skip all uninitalized nodes.
      if (!IsStrategyInitializedForMyHand(match.matched_node_, type, new_match_state)) {
        match.Print("skip virgin node in new ag range estimate: ");
        continue;
      }
    }
    //start
    match.Print("try matched node in new ag reach estimate: ");
    std::array<sHandBelief, 2> local_base_reach;
    local_base_reach[0].CopyValue(&new_base_reach[0]);
    local_base_reach[1].CopyValue(&new_base_reach[1]);
    auto estimate_return_code = EstimateReachProbAtNode(new_match_state,
                                                        match.matched_node_,
                                                        local_base_reach,
                                                        type,
                                                        DEFAULT_BAYESIAN_TRANSITION_FILTER);
    if (estimate_return_code == RANGE_ESTIMATE_SUCCESS) {
      //direct copy first. then normalize it.
      new_ag->root_hand_belief_[0].CopyValue(&local_base_reach[0]);
      new_ag->root_hand_belief_[1].CopyValue(&local_base_reach[1]);
      new_ag->NormalizeRootReachProb();

      //print root belief for debugging
      new_ag->root_hand_belief_[0].PrintNonZeroBelief();
      new_ag->root_hand_belief_[1].PrintNonZeroBelief();

      return true;
    } else {
      bool is_street_root = new_ag->root_state_.numActions[new_ag->root_state_.round] == 0;
      logger::warn("   reach estimate %d -> failed [ %s ] at [ %s ] at [round %d]. try next",
                   attempt_cursor,
                   RangeEstimateCodeMap[estimate_return_code],
                   is_street_root ? "street_root" : "non_street_root", new_round);
      attempt_cursor++;
    }
  }
  return false;
}

void Strategy::PickAction(MatchState *match_state,
                          ActionChooser *action_chooser,
                          Action &r_action,
                          STRATEGY_TYPE calc_mode,
                          Node *matched_node) {
  //compute r n b
  int round = matched_node->GetRound();
  Bucket_t b = GetBucketFromMatchState(match_state);
  int n = matched_node->GetN();
  int a_max = matched_node->children.size();

  //get strategy
  float rnb_avg[a_max];
  ComputeStrategy(round, n, b, a_max, rnb_avg, calc_mode);

  //evict invalid actions by setting avg[a] = 0;
  for (int i = 0; i < a_max; i++) {
    auto action = matched_node->children[i]->GetLastAction();
    if (!isValidAction(&ag_->game_, &match_state->state, 1, &action)) {
      logger::warn(
          "matched node has un-fixable action %c%d at r = %d | force evict this action | add its prob to call (pending fix)",
          actionChars[action.type],
          action.size,
          matched_node->GetRound());
      for (auto a = 0; a < a_max; a++) {
        //find call
        if (matched_node->children[a]->GetLastActionCode() == 0) {
          rnb_avg[a] += rnb_avg[i];
          break;
        }
      }
      rnb_avg[i] = 0;
    }
  }

  // likely due to 0 reach. but how's that
  if (IsAvgUniform(rnb_avg, a_max)) {
    logger::warn("choosing from uniform strategy");
    //print regret and wavg
    PrintNodeStrategy(matched_node, b, calc_mode);
  }

  //pick an action
  action_chooser->ChooseAction(rnb_avg, a_max, matched_node, r_action);
  //print the whole straetgy profile for debugging
  PrintNodeStrategy(matched_node, b, calc_mode);
}

/**
 * todo: make all rounds use range to represent the data. i.e. merge them by preflop colex.
 */
void Strategy::InspectNode(Node *inspect_node, const std::string &prefix, STRATEGY_TYPE calc_mode) {
  std::string filename = prefix + ".csv";

  Board_t board{};
  BoardFromState(&ag_->game_, &inspect_node->state_, &board);

  Round_t r = inspect_node->GetRound();
  Node_t n = inspect_node->GetN();

  std::filesystem::path dir(BULLDOG_DIR_DATA_LAB);
  FILE *file = fopen((dir / filename).c_str(), "wb");
  if (file != nullptr) {
    fprintf(file, "colex,suited,paired,card1,card2,hand+board,rank");
    for (auto a = 0; a < inspect_node->GetAmax(); a++)
      fprintf(file, ",%d", inspect_node->children[a]->GetLastActionCode());
    fprintf(file, ",raise_sum");
    fprintf(file, "\n");
    std::set<Colex> seen_colex;
    int counter = 0;
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
      auto high_low = FromVectorIndex(i);
      if (board.CardCrash(high_low.first) || board.CardCrash(high_low.second))
        continue;
      auto c = emptyCardset();
      EnrichCardSetToRound(&c, high_low.first, high_low.second, &board, r);
      Colex colex = ComputeColex(Canonize(c.cards));
      if (seen_colex.find(colex) != seen_colex.end())
        continue;  //continue if founded

      //not found, add to it.
      seen_colex.insert(colex);

      auto flop = emptyCardset();
      Bucket_t b = ag_->bucket_reader_.GetBucket(&c, &flop, r);
      Action_t a_max = ag_->kernel_->amax_by_rn_[r][n];
      float rnb_avg[a_max];
      ComputeStrategy(r, n, b, a_max, rnb_avg, calc_mode);

      // prepare for printing
      bool suited = suitOfCard(high_low.first, 4) == suitOfCard(high_low.second, 4);
      bool paired = rankOfCard(high_low.first, 4) == rankOfCard(high_low.second, 4);
      auto c1 = emptyCardset();
      AddCardTToCardset(&c1, high_low.first);
      auto c2 = emptyCardset();
      AddCardTToCardset(&c2, high_low.second);
      fprintf(file,
              "colex%d,%d,%d,%s,%s,%s,%d",
              counter++,
              suited,
              paired,
              CardsToString(c1.cards).c_str(),
              CardsToString(c2.cards).c_str(),
              CardsToString(c.cards).c_str(),
              RankHand(high_low.first, high_low.second, &board));
      for (auto a = 0; a < a_max; a++) {
        fprintf(file, ",%f", rnb_avg[a]);
      }
      auto raise_sum = 0.0;
      //if fold, 2 else 1
      int raise_starting_idx = inspect_node->children[0]->GetLastActionCode() == -1 ? 2 : 1;
      for (auto a = raise_starting_idx; a < a_max; a++)
        raise_sum += rnb_avg[a];
      fprintf(file, ",%f", raise_sum);
      fprintf(file, "\n");
    }
    fclose(file);
    logger::info("inspection data saved to %s", filename);
  } else {
    logger::error("unable to open file %s", filename);
  }
}

void Strategy::PrintNodeStrategy(Node *node, Bucket_t b, STRATEGY_TYPE calc_mode) const {
  auto r = node->GetRound();
  auto n = node->GetN();
  auto a_max = node->GetAmax();
  float rnb_avg[a_max];
  ComputeStrategy(r, n, b, a_max, rnb_avg, calc_mode);
  std::string print_string = "strategy profile : ";
  for (auto a_2 = 0; a_2 < a_max; a_2++) {
    print_string += std::to_string(node->children[a_2]->GetLastActionCode());
    print_string += ": ";
    print_string += std::to_string(rnb_avg[a_2] * 100);
    print_string += "%";
    print_string += " | ";
  }
  logger::debug("%s | for r = %d | n = %d | b = %d", print_string, r, n, b);
}

void Strategy::InspectStrategyByMatchState(MatchState *match_state, STRATEGY_TYPE calc_mode) {
  //perform action mapping
  NodeMatchCondition condition;
  ag_->MapToNode(match_state->state, condition);
  auto matched_node = condition.matched_node_;

  //compute r n b
  int round = matched_node->GetRound();
  auto viewing_player = match_state->viewingPlayer;
  auto c_1 = match_state->state.holeCards[viewing_player][0];
  auto c_2 = match_state->state.holeCards[viewing_player][1];
  Card_t high, low;
  if (c_1 > c_2) {
    high = c_1;
    low = c_2;
  } else {
    high = c_2;
    low = c_1;
  }
  Board_t board{};
  BoardFromState(&ag_->game_, &match_state->state, &board);
  auto b = ag_->bucket_reader_.GetBucketWithHighLowBoard(high, low, &board, round);
  PrintNodeStrategy(matched_node, b, calc_mode);
}

int Strategy::EstimateReachProbAtNode(MatchState *last_match_state,
                                      Node *target_node,
                                      std::array<sHandBelief, 2> &new_ag_reach,
                                      STRATEGY_TYPE calc_mode,
                                      double min_filter) const {

  ag_->root_node_->PrintState("[range estimate] from state --> ");
  target_node->PrintState("[range estimate] to state --> ");
  SimpleTimer timer;

  //common value
  auto root_round = ag_->root_node_->GetRound();
  Board_t reach_board{};
  BoardFromState(&ag_->game_, &last_match_state->state, &reach_board);

  //refine reach that crashes with board. do it once here.
  new_ag_reach[0].ExcludeBoard(reach_board);
  new_ag_reach[1].ExcludeBoard(reach_board);

  int hero_pos = last_match_state->viewingPlayer;
  auto my_hand_vector_idx = ToVectorIndex(last_match_state->state.holeCards[hero_pos][0],
                                          last_match_state->state.holeCards[hero_pos][1]);

  //find the travel path from ag.root to matched node;
  std::stack<int> node_path = target_node->GetPathFromRoot();
  if (node_path.empty()) {
    logger::warn("no step nodes to the root. already at root");
  }
  Node *stepping_node = ag_->root_node_;
  int step_size = node_path.size();
  int done_step_count = 0;
  while (!node_path.empty()) {
    auto action_idx = node_path.top();
    if (action_idx == -1) {
      logger::warn("node path wrong. cannot find the matched node. matched to terminal node");
      return false;
    }
    auto action_code = stepping_node->children[action_idx]->GetLastActionCode();
    auto n = stepping_node->GetN();
    int acting_player = stepping_node->GetActingPlayer();
    auto step_node_round = stepping_node->GetRound();
    //tallying parameters
    int prune_count = 0;

    //do bayesian estimate on each ligit hand
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
      if (new_ag_reach[acting_player].IsZero(i) || new_ag_reach[acting_player].IsPruned(i))
        continue;

      auto high_low = FromVectorIndex(i);
      auto b =
          ag_->bucket_reader_.GetBucketWithHighLowBoard(high_low.first, high_low.second, &reach_board, step_node_round);
      auto a_max = stepping_node->children.size();

      //use only zipavg/wavg in transition.
      float rnb_avg[a_max];
      ComputeStrategy(step_node_round, n, b, a_max, rnb_avg, calc_mode);
      float action_prob = 0.0;

      //a uniform strategy means likely the reach of a node of the same acting player (either at root or other step nodes) is 0. So it should not happen much
      if (!IsAvgUniform(rnb_avg, a_max)){
        action_prob = rnb_avg[action_idx];
      } else {
        //what"?
        logger::warn("weird uniform strategy in transition. the reach should be 0 already and filtered at start.");
      }


      //perform heuristically pruning in bayesian estimation. incomplete convergence.
      if (action_prob < min_filter) {
        //safe check
        if (hero_pos == acting_player && i == my_hand_vector_idx) {
          logger::debug("[player %d] [real hand %d %s] was pruned [%f < %f]",
                        hero_pos, my_hand_vector_idx, VectorIdxToString(my_hand_vector_idx), action_prob, min_filter);
          PrintNodeStrategy(stepping_node, b, calc_mode);
          return REAL_HAND_PRUNED_IN_TRANSITION;
        }
        if (root_round > HOLDEM_ROUND_PREFLOP) //not likely to have zero wavg at preflop
          prune_count++;
        action_prob = 0.0;
      }
      new_ag_reach[acting_player].belief_[i] *= action_prob;
    }

    logger::debug(
        "    step %d -> [player %d] [action_code %d] [at round %d] [%d hands pruned]",
        done_step_count + 1,
        acting_player,
        action_code,
        step_node_round,
        prune_count);

    if (new_ag_reach[acting_player].BeliefSum() == 0.0) {
      logger::debug(
          "    range estimate failed --> [player %d] [fails at %d/%d steps (action %d) from round %d to %d]",
          acting_player,
          done_step_count + 1,
          step_size,
          action_code,
          root_round,
          target_node->GetRound());
      return acting_player == hero_pos ? ALL_HERO_HAND_PRUNED_IN_TRANSITION : ALL_OPP_HAND_PRUNED_IN_TRANSITION;
    }

    stepping_node = stepping_node->children[action_idx];
    node_path.pop();
    done_step_count++;
  }

  if (step_size != done_step_count)
    logger::warn("the transition path is not legal. Check it.");

  logger::debug("range estimate success [path size %d] [round %d to %d]",
                step_size,
                root_round,
                target_node->GetRound());
  timer.Checkpoint("reach estimate");
  return RANGE_ESTIMATE_SUCCESS;
}

//todo: test code
Bucket_t Strategy::GetBucketFromMatchState(MatchState *match_state) const {
  auto viewing_player = match_state->viewingPlayer;
  auto c_1 = match_state->state.holeCards[viewing_player][0];
  auto c_2 = match_state->state.holeCards[viewing_player][1];
  Board_t board{};
  BoardFromState(&ag_->game_, &match_state->state, &board);
  return ag_->bucket_reader_.GetBucketWithHighLowBoard(c_1, c_2, &board, match_state->state.round);
}

void Strategy::AllocateMemory(STRATEGY_TYPE type, CFR_MODE cfr_mode) {
  RNBA size = ag_->kernel_->MaxIndex();
  int bytesize = 1;
  switch (type) {
    case STRATEGY_REG:
      if (cfr_mode == CFR_VECTOR_ALTERNATE_SOLVE || cfr_mode == CFR_VECTOR_PAIRWISE_SOLVE) {
        double_regret_ = new double[size];
        bytesize = 8;
      } else if (cfr_mode == CFR_SCALAR_SOLVE) {
        // scalar
        int_regret_ = new int[size];
        bytesize = 4;
      } else {
        logger::critical("unsupported cfr type now %d", cfr_mode);
      }
      break;
    case STRATEGY_WAVG:
      if (cfr_mode == CFR_VECTOR_ALTERNATE_SOLVE || cfr_mode == CFR_VECTOR_PAIRWISE_SOLVE) {
        ulong_wavg_ = new uint64_t[size];
        bytesize = 8;
      } else if (cfr_mode == CFR_SCALAR_SOLVE) {
        // scalar
        uint_wavg_ = new uint32_t[size];
        bytesize = 4;
      } else {
        logger::critical("unsupported cfr type now %d", cfr_mode);
      }
      break;
    case STRATEGY_ZIPAVG:
      bytesize = 1;
      zipavg_ = new ZIPAVG[size];
      break;
    default:
      logger::critical("unsupported strategy type %s", StrategyToNameMap[type]);
  }
  double mem_size = (double) size * bytesize / (1024.0 * 1024.0);
  logger::debug("allocated heap memory for %s || length = %d || size = %f (mb)", StrategyToNameMap[type], size,
                mem_size);
}

void Strategy::DiscountStrategy(STRATEGY_TYPE type, double factor) const {
  switch (type) {
    case STRATEGY_REG:
      if (double_regret_ != nullptr) {
        for (RNBA i = 0; i < ag_->kernel_->MaxIndex(); i++) {
          double_regret_[i] *= factor;
        }
      } else {
        //must be int regret then
        for (RNBA i = 0; i < ag_->kernel_->MaxIndex(); i++) {
          INT_REGRET new_v = (int) (int_regret_[i] * factor);
          int_regret_[i] = new_v;
        }
      }
      break;
    case STRATEGY_WAVG:
      if (ulong_wavg_ != nullptr) {
        for (RNBA i = 0; i < ag_->kernel_->round_index_0_[1]; i++) {
          double new_weighted_avg = ulong_wavg_[i] * factor;
          ulong_wavg_[i] = (ULONG_WAVG) new_weighted_avg;
        }
      } else {
        //must be int wavg
        for (RNBA i = 0; i < ag_->kernel_->round_index_0_[1]; i++) {
          UINT_WAVG new_weighted_avg = uint_wavg_[i] * factor;
          uint_wavg_[i] = (UINT_WAVG) new_weighted_avg;
        }
      }
      break;
    default:
      logger::critical("unsupported strategy type for discounting %s", StrategyToNameMap[type]);
  }
  logger::trace("discounting %s with factor = %f", StrategyToNameMap[type], factor);
}

int Strategy::ComputeStrategy(Node *node, Bucket_t b, float *rnb_avg, STRATEGY_TYPE mode) const {
  return ComputeStrategy(node->GetRound(), node->GetN(), b, node->GetAmax(), rnb_avg, mode);
}

int Strategy::ComputeStrategy(Round_t r,
                              Node_t n,
                              Bucket_t b,
                              Action_t a_max,
                              float *rnb_avg,
                              STRATEGY_TYPE mode) const {
  auto rnb0 = ag_->kernel_->hash_rnba(r, n, b, 0);
  switch (mode) {
    case STRATEGY_WAVG: {
      if (ulong_wavg_ != nullptr) {
        return GetPolicy<ULONG_WAVG>(rnb_avg, a_max, ulong_wavg_ + rnb0);
      } else {
        return GetPolicy<UINT_WAVG>(rnb_avg, a_max, uint_wavg_ + rnb0);
      }
    }
    case STRATEGY_REG: {
      if (double_regret_ != nullptr) {
        return GetPolicy<DOUBLE_REGRET>(rnb_avg, a_max, double_regret_ + rnb0);
      } else {
        return GetPolicy<INT_REGRET>(rnb_avg, a_max, int_regret_ + rnb0);
      }
    }
    case STRATEGY_ZIPAVG: {
      if (zipavg_ != nullptr) {
        return GetPolicy<ZIPAVG>(rnb_avg, a_max, zipavg_ + rnb0);
      } else {
        char v[a_max];
        file_ptr->seekg(rnb0);
        file_ptr->read(v, a_max);
        ZIPAVG zip_v[a_max];
        for (auto i = 0; i < a_max; i++)
          zip_v[i] = v[i];
        return GetPolicy<ZIPAVG>(rnb_avg, a_max, zip_v);
      }
    }
    default:
      logger::critical("does not support this cal stratey mode %s", StrategyToNameMap[mode]);
      break;
  }
#if DEV > 1
  CheckAvgSum(rnb_avg, a_max);
#endif
  return 0;
}

void Strategy::ClearZipAvgMemory() {
  delete[] zipavg_;
  zipavg_ = nullptr;
}

void Strategy::ConvertWavgToZipAvg(pthread_t *thread_pool, unsigned int num_threads) const {
  logger::debug("converting wavg to zipavg with %d threads", num_threads);
  for (auto r = 0; r < HOLDEM_MAX_ROUNDS; r++) {
    auto b_max = ag_->kernel_->bmax_by_r_[r];
    if (b_max < num_threads) {
      logger::critical("b_max %d must >= numthread %d", b_max, num_threads);
    }
    unsigned int step = floor((double) b_max / num_threads);

    //launch threads
    for (unsigned int i = 0; i < num_threads; i++) {
      Bucket_t b_begin = i * step;
      //force the last to be b_max. so the last may solve with more
      Bucket_t b_end = (i == num_threads - 1) ? b_max : (i + 1) * step;
      auto thread_input = new sThreadInputZipavgConvert(this, b_begin, b_end, r);
      logger::debug("thread %d converting from %d to %d", i, b_begin, b_end);
      if (pthread_create(&thread_pool[i], nullptr, Strategy::ThreadedZipAvgConvert, thread_input)) {
        logger::critical("failed to launch threads.");
      }
    }
    // wait for threads to finish
    for (unsigned int i = 0; i < num_threads; ++i) {
      if (pthread_join(thread_pool[i], nullptr)) {
        logger::error("Couldn't join to thread %d", i);
      }
    }
  }
}

/*
 * given r, b_range, compute it
 */
void *Strategy::ThreadedZipAvgConvert(void *thread_args) {
  auto *args = (sThreadInputZipavgConvert *) thread_args;
  auto strategy = args->strategy;
  auto r = args->round;
  for (Bucket_t b = args->b_begin; b < args->b_end; b++)
    for (Node_t n = 0; n < strategy->ag_->kernel_->nmax_by_r_[r]; n++) {
      auto a_max = strategy->ag_->kernel_->amax_by_rn_[r][n];
      float avg[a_max];
      strategy->ComputeStrategy(r, n, b, a_max, avg, STRATEGY_WAVG);
      //map to uint, normalized by 250, where uint8_max = 256.
      for (auto a = 0; a < a_max; a++) {
        auto rnba = strategy->ag_->kernel_->hash_rnba(r, n, b, a);
        ZIPAVG v = std::round(avg[a] * 250);
        strategy->zipavg_[rnba] = v;
      }
    }
  delete args;
  return nullptr;
}

/*
 * classic inspection
 */
void Strategy::InspectPreflopBets(const std::string &print_name, STRATEGY_TYPE calc_mode) {
//the classic, open_3bet_4bet
  InspectNode(ag_->root_node_, print_name + "_open", calc_mode);
  Node *three_bet_node = nullptr;
  for (auto c : ag_->root_node_->children) {
    if (c->GetLastActionCode() == 200)
      three_bet_node = c;
  }
  /*
   * 3bet
   */
  if (three_bet_node != nullptr) {
    InspectNode(three_bet_node, print_name + "_3bet", calc_mode);
    //find raise 600
    Node *four_bet_node = nullptr;
    for (auto c : three_bet_node->children) {
      if (c->GetLastActionCode() == 600)
        four_bet_node = c;
      /*
       * 4bet
       */
      if (four_bet_node != nullptr) {
        InspectNode(four_bet_node, print_name + "_4bet", calc_mode);
        Node *five_bet_node = nullptr;
        for (auto c4 : four_bet_node->children) {
          if (c4->GetLastActionCode() == 1800)
            five_bet_node = c4;
          if (five_bet_node != nullptr) {
            /*
             * 5bet
             */
            InspectNode(five_bet_node, print_name + "_5bet", calc_mode);
            Node *six_bet_node = nullptr;
            for (auto c5 : five_bet_node->children) {
              if (c5->GetLastActionCode() == 5400)
                six_bet_node = c5;
              if (six_bet_node != nullptr) {
                /*
                 * 6bet
                 */
                InspectNode(six_bet_node, print_name + "_6bet", calc_mode);
                Node *seven_bet_node = nullptr;
                for (auto c6 : six_bet_node->children) {
                  if (c6->GetLastActionCode() == 10000)
                    seven_bet_node = c6;
                  if (seven_bet_node != nullptr) {
                    /*
                     * 6bet
                     */
                    InspectNode(seven_bet_node, print_name + "_7bet", calc_mode);

                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

/*
 * can only free the strategy of this round.
 * copy the last state over.
 * - find the actions taken by the viewing player
 * - match that state to both ag, and check the ligitimicy
 * - clone the strategy over
 * - free the nodes in the new strategy
 * TODO： should it only use initialized node in maptonode
 */
bool Strategy::FreezePriorAction(Strategy *old_strategy, MatchState *real_match_state) const {
  logger::require_critical(ag_->root_node_->GetRound() != old_strategy->ag_->root_node_->GetRound(),
                           "can not freeze with reference strategy from prior rounds [old round %d] [new round %d]",
                           old_strategy->ag_->root_node_->GetRound(),
                           ag_->root_node_->GetRound());

  //find the actions taken
  int r = real_match_state->state.round;
  uint8_t a_max_state = real_match_state->state.numActions[r];
  logger::require_critical(a_max_state < 2, "with less than 2 actions!  impossible for 2p");

  auto viewing_player = real_match_state->viewingPlayer;
  std::vector<int> freeze_action_index;
  for (auto a = 0; a < a_max_state; a++) {
    if (real_match_state->state.actingPlayer[r][a] == viewing_player)
      freeze_action_index.emplace_back(a);
  }

  /*
   * match them and free them
   */
  for (auto freeze_index : freeze_action_index) {
    //e.g. r300r900r1800, a_max = 3. hero acts first, that free_index = [0, 2]
    int steps_to_reverse = a_max_state - freeze_index;
    State step_back_state;
    if (StepBackAction(&ag_->game_, &real_match_state->state, &step_back_state, steps_to_reverse) == -1) {
      logger::critical("stepping too many steps. u may have an empty state or invalid state. return false");
    };
    //match to both tree
    NodeMatchCondition old_match_condition;
    old_strategy->ag_->MapToNode(step_back_state, old_match_condition);
    auto old_match_node = old_match_condition.matched_node_;
    NodeMatchCondition new_match_condition;
    ag_->MapToNode(step_back_state, new_match_condition);
    auto new_match_node = new_match_condition.matched_node_;
    if (old_match_node->GetAmax() != new_match_node->GetAmax()) {
      logger::error("tough to map strategy over with diff actions size");
      return false;
    }

    if (!StateBettingEqual(&old_match_node->state_, &new_match_node->state_)) {
      old_match_node->PrintState("old_strategy node = ");
      new_match_node->PrintState("new_strategy node = ");
      logger::error("should match to the nodes with the same betting sequence");
      return false;
    }

    //copy the wavg over, assuming we only use cfr+ for solving for simplicity
    if (ulong_wavg_ == nullptr || old_strategy->ulong_wavg_ == nullptr) {
      logger::error("freezing strategy only support cfr+ based");
      return false;
    }

    auto new_b = GetBucketFromMatchState(real_match_state);
    new_match_node->frozen_b = new_b;
    auto hand = ToVectorIndex(real_match_state->state.holeCards[viewing_player][0],
                              real_match_state->state.holeCards[viewing_player][1]);
    logger::debug("freeze [b %d] [hand %s] at :", new_b, VectorIdxToString(hand));
    new_match_node->PrintState("frozen state : ");
    auto old_b = old_strategy->GetBucketFromMatchState(real_match_state);
    auto new_n = new_match_node->GetN();
    auto old_n = old_match_node->GetN();
    auto a_max = old_match_node->GetAmax();
    for (auto a = 0; a < a_max; a++) {
      auto new_rnba = ag_->kernel_->hash_rnba(r, new_n, new_b, a);
      auto old_rnba = old_strategy->ag_->kernel_->hash_rnba(r, old_n, old_b, a);
      ulong_wavg_[new_rnba] = old_strategy->ulong_wavg_[old_rnba];
    }

#if DEV > 1
    //debug code, make sure both strategy are the same
    float old_avg[a_max];
    old_strategy->ComputeStrategy(old_match_node, old_b, old_avg, STRATEGY_WAVG);
    float new_avg[a_max];
    ComputeStrategy(new_match_node, old_b, new_avg, STRATEGY_WAVG);
    for (auto a = 0; a < a_max; a++) {
      if (abs(old_avg[a] - new_avg[a]) > 0.00001)
        logger::critical("copying strategy over failed [old %f][new %f][a = %d]", old_avg[a], new_avg[a], a);
    }
    old_strategy->PrintNodeStrategy(old_match_node, old_b, STRATEGY_WAVG);
#endif
    //done copying, go to the next frozen action
  }
  return true;
}

void Strategy::CheckFrozenStrategyConsistency(Strategy *old_strategy, MatchState *real_match_state) const {
  //find the actions taken
  int r = real_match_state->state.round;
  uint8_t a_max_state = real_match_state->state.numActions[r];
  logger::require_critical(a_max_state < 2, "with less than 2 actions!  impossible for 2p");

  auto viewing_player = real_match_state->viewingPlayer;
  std::vector<int> freeze_action_index;
  for (auto a = 0; a < a_max_state; a++) {
    if (real_match_state->state.actingPlayer[r][a] == viewing_player)
      freeze_action_index.emplace_back(a);
  }

  /*
   * match them and free them
   */
  for (auto freeze_index : freeze_action_index) {
    //e.g. r300r900r1800, a_max = 3. hero acts first, that free_index = [0, 2]
    int steps_to_reverse = a_max_state - freeze_index;
    State step_back_state;
    logger::require_critical(
        StepBackAction(&ag_->game_, &real_match_state->state, &step_back_state, steps_to_reverse) == -1,
        "stepping too many steps. u may have an empty state or invalid state. return false");
    //match to both tree
    NodeMatchCondition old_match_condition;
    old_strategy->ag_->MapToNode(step_back_state, old_match_condition);
    auto old_match_node = old_match_condition.matched_node_;
    NodeMatchCondition new_match_condition;
    ag_->MapToNode(step_back_state, new_match_condition);
    auto new_match_node = new_match_condition.matched_node_;
    logger::require_critical(old_match_node->GetAmax() != new_match_node->GetAmax(),
                             "tough to map strategy over with diff actions size");

    if (!StateBettingEqual(&old_match_node->state_, &new_match_node->state_)) {
      old_match_node->PrintState("old_strategy node = ");
      new_match_node->PrintState("new_strategy node = ");
      logger::critical("should match to the nodes with the same betting sequence");
    }

    //copy the wavg over, assuming we only use cfr+ for solving for simplicity
    logger::require_critical(ulong_wavg_ == nullptr || old_strategy->ulong_wavg_ == nullptr,
                             "freezing strategy only support cfr+ based");

    auto new_b = GetBucketFromMatchState(real_match_state);
    auto hand = ToVectorIndex(real_match_state->state.holeCards[viewing_player][0],
                              real_match_state->state.holeCards[viewing_player][1]);
    logger::debug("freeze [b %d] [hand %s] at :", new_b, VectorIdxToString(hand));
    new_match_node->PrintState("frozen state : ");
    auto old_b = old_strategy->GetBucketFromMatchState(real_match_state);
    auto a_max = new_match_node->GetAmax();
    //debug code, make sure both strategy are the same
    float old_avg[a_max];
    old_strategy->ComputeStrategy(old_match_node, old_b, old_avg, STRATEGY_WAVG);
    float new_avg[a_max];
    ComputeStrategy(new_match_node, old_b, new_avg, STRATEGY_WAVG);
    for (auto a = 0; a < a_max; a++) {
      if (abs(old_avg[a] - new_avg[a]) > 0.00001)
        logger::critical("copying strategy over failed [old %f][new %f][a = %d]", old_avg[a], new_avg[a], a);
    }
    old_strategy->PrintNodeStrategy(old_match_node, old_b, STRATEGY_WAVG);
  }
}

ZIPAVG Strategy::GetZipAvg(RNBA idx) const {
  if (zipavg_ != nullptr)
    return zipavg_[idx];
  //else, disk-based reading
  char ch;
  file_ptr->seekg(idx);
  file_ptr->get(ch);
  return (ZIPAVG) ch;
}

bool Strategy::IsStrategyInitializedForMyHand(Node *matched_node,
                                              STRATEGY_TYPE strategy_type,
                                              MatchState *match_state) const {
  auto bucket = GetBucketFromMatchState(match_state);
  auto a_max = matched_node->GetAmax();
  float avg[a_max];
  ComputeStrategy(matched_node, bucket, avg, strategy_type);
  bool uniform = IsAvgUniform(avg, a_max);
  if (uniform) {
    PrintNodeStrategy(matched_node, bucket, strategy_type);
  }
  return !uniform;
}

std::vector<NodeMatchCondition> Strategy::FindSortedMatchedNodes(State &state) const {
  if (ag_->node_map_.empty()) ag_->IndexBettingTree();

  auto current_player = currentPlayer(&ag_->game_, &state);
  auto real_betting = GetBettingStr(&ag_->game_, state);
  auto all_nodes = ag_->node_map_[state.round].equal_range(current_player);

  SimpleTimer timer;
  std::vector<NodeMatchCondition> candidate_conditions;
  for (auto it = all_nodes.first; it != all_nodes.second; it++) {
    auto node = (*it).second;
    NodeMatchCondition new_condition(state, node);
    if (new_condition.bet_sim_dist_ > 2)
      continue;
    candidate_conditions.push_back(new_condition);
  }
  std::sort(candidate_conditions.begin(), candidate_conditions.end());
//  timer.Checkpoint("selecting match node candidates");
  //for extreme case where you have none matched nodes
  logger::require_critical(!candidate_conditions.empty(), "none node matched. terrible tree");
  return candidate_conditions;
}