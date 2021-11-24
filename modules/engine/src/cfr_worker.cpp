//
// Created by Isaac Zhang on 3/20/20.
//

#include "cfr_worker.h"
#include "node.h"

//todo: add back the pruning is necessary
/*
 * we ignore those prune hands anyway, so no need to set -1. the program wont crash
 */
sHandBelief *VectorCfrWorker::WalkTree_Alternate(Node *this_node, int actor, sHandBelief *opp_belief) {
  if (opp_belief->AllZero())
    return new sHandBelief(0.0);

  if (this_node->IsTerminal()) {
    auto cfu = new sHandBelief(0.0);
    hand_kernel->hand_eval_kernel_.FastTerminalEval(opp_belief->belief_,
                                                    cfu->belief_,
                                                    this_node->GetStake(actor),
                                                    this_node->IsShowdown());
    return cfu;
  }
  return EvalChoiceNode_Alternate(this_node, actor, opp_belief);
}

sHandBelief *VectorCfrWorker::EvalChoiceNode_Alternate(Node *this_node, int trainee, sHandBelief *opp_belief) {
  auto a_max = this_node->GetAmax();
  bool is_my_turn = trainee == this_node->GetActingPlayer();
  /*
   * ROLLOUT
   */
  //copy the reach_ranges to child ranges
  std::vector<sHandBelief *> child_reach_ranges;
  child_reach_ranges.reserve(a_max);
  if (is_my_turn) {
    //just copy the pointer
    for (int a = 0; a < a_max; a++)
      child_reach_ranges.emplace_back(opp_belief);

  } else {
    //copy the object and rollout
    for (int a = 0; a < a_max; a++)
      child_reach_ranges.emplace_back(new sHandBelief(opp_belief));
    RangeRollout(this_node, opp_belief, child_reach_ranges);
  }

  /*
   * WALK TREE RECURSE DOWN
   */
  std::vector<sHandBelief *> child_cfu;
  child_cfu.reserve(a_max);
  for (int a = 0; a < a_max; a++)
    child_cfu.emplace_back(WalkTree_Alternate(this_node->children[a], trainee, child_reach_ranges[a]));

  /*
   * COMPUTE CFU
   */
  //precompute strategy if my turn only. range rollout is fine, it is compute on the fly
  float *avg_all = nullptr;
  if (is_my_turn) {
    //unless you have pruning, or you dont need to skip any. it is a bit waste but makes it more accurate.
    avg_all = new float[FULL_HAND_BELIEF_SIZE * a_max];
    for (auto &i : hand_kernel->valid_index_) {
      int offset = a_max * i;
      auto b = hand_kernel->GetBucket(this_node->GetRound(), i);
      strategy_->ComputeStrategy(this_node, b, avg_all + offset, cfr_param_->strategy_cal_mode_);
    }
  }

  auto cfu = new sHandBelief(0.0);
  CFU_COMPUTE_MODE mode = is_my_turn ? cfr_param_->cfu_compute_acting_playing : cfr_param_->cfu_compute_opponent;
  ComputeCfu(this_node, child_reach_ranges, child_cfu, cfu, mode, avg_all);

  /*
   * REGRET LEARNING
   */
  //only learning on the trainee's node
  if (cfr_param_->regret_learning_on)
    if (is_my_turn)
      RegretLearning(this_node, child_cfu, cfu);

  //delete child pop up cfu
  for (int a = 0; a < a_max; a++) {
    delete child_cfu[a];
    if (!is_my_turn)
      delete child_reach_ranges[a];
  }
  if (is_my_turn)
    delete[] avg_all;

  return cfu;
}

Ranges *VectorCfrWorker::WalkTree_Pairwise(Node *this_node, Ranges *reach_ranges) {
  if (reach_ranges->ReturnReady(iter_prune_flag))
    return new Ranges(reach_ranges, "empty");

  if (this_node->IsTerminal()) {
    //some range are 0. do some early return processing
    auto this_cfu = new Ranges(reach_ranges); //copy the maskes as well.
    for (auto my_pos = 0; my_pos < reach_ranges->num_player_; my_pos++) {
      hand_kernel->hand_eval_kernel_.FastTerminalEval(reach_ranges->beliefs_[1 - my_pos].belief_,
                                                      this_cfu->beliefs_[my_pos].belief_,
                                                      this_node->GetStake(my_pos),
                                                      this_node->IsShowdown());
    }
#if DEV > 1
    for (int p = 0; p < 2; p++)
      if (!this_cfu->beliefs_[p].TopoAligned(&reach_ranges->beliefs_[p]))
        logger::error("misaligned belief %d topo %d != %d", p,
                      this_cfu->beliefs_[p].CountPrunedEntries(),
                      reach_ranges->beliefs_[p].CountPrunedEntries());
#endif
    return this_cfu;
  }

  return EvalChoiceNode_Pairwise(this_node, reach_ranges);
}

Ranges *VectorCfrWorker::EvalChoiceNode_Pairwise(Node *this_node, Ranges *reach_ranges) {
  auto a_max = this_node->GetAmax();
  /*
   * ROLLOUT
   */
  //copy the reach_ranges to child ranges for both players
  std::vector<Ranges *> child_reach_ranges;
  child_reach_ranges.reserve(a_max);
  for (int a = 0; a < a_max; a++)
    child_reach_ranges.emplace_back(new Ranges(reach_ranges));

  //only need to rollout the acting_player
  int actor = this_node->GetActingPlayer();
  auto actor_child_beliefs = ExtractBeliefs(child_reach_ranges, actor);
  RangeRollout(this_node, &reach_ranges->beliefs_[actor], actor_child_beliefs);

  /*
   * WALK TREE RECURSE DOWN
   */
  std::vector<Ranges *> child_cfu;
  child_cfu.reserve(a_max);
  for (int a = 0; a < a_max; a++)
    child_cfu.emplace_back(WalkTree_Pairwise(this_node->children[a], child_reach_ranges[a]));

  /*
   * COMPUTE CFU
   */
  auto cfu = new Ranges(reach_ranges, "empty");
  //acting player
  auto actor_child_cfu = ExtractBeliefs(child_cfu, actor);
  //todo: need to add the memory thing here.
  ComputeCfu(this_node,
             actor_child_beliefs,
             actor_child_cfu,
             &cfu->beliefs_[actor],
             cfr_param_->cfu_compute_acting_playing, nullptr);
  //opponent
  int opp_pos = 1 - actor;
  ComputeCfu(this_node,
             ExtractBeliefs(child_reach_ranges, opp_pos),
             ExtractBeliefs(child_cfu, opp_pos),
             &cfu->beliefs_[opp_pos],
             cfr_param_->cfu_compute_opponent, nullptr);

  /*
   * REGRET LEARNING
   */
  //only on actor
  if (cfr_param_->regret_learning_on)
    RegretLearning(this_node, actor_child_cfu, &cfu->beliefs_[actor]);

  //delete the util belief from child nodes
  for (int a = 0; a < a_max; a++) {
    // pop up
    delete child_cfu[a];
    //goes down
    delete child_reach_ranges[a];
  }
  return cfu;
}

/*
 * - do range update, and do pruning
 * - do wavg update
 * on both side
 */
void VectorCfrWorker::RangeRollout(Node *this_node, sHandBelief *range, std::vector<sHandBelief *> &child_ranges) {
  auto r = this_node->GetRound();
  auto a_max = this_node->GetAmax();
  auto n = this_node->GetN();
  auto frozen_b = this_node->frozen_b;

  for (auto &i : hand_kernel->valid_index_) {
    //greedily outer skip the 0 belief and board cards
    if (range->IsPruned(i))
      continue;

    //also if 0
    if (range->IsZero(i))
      continue;

    auto b = hand_kernel->GetBucket(r, i);
    auto rnb0 = strategy_->ag_->kernel_->hash_rnba(r, n, b, 0);
    float rnb_avg[a_max];
    strategy_->ComputeStrategy(this_node, b, rnb_avg, cfr_param_->strategy_cal_mode_);

    /*
     * update WAVG, if not frozen.
     */
    if (cfr_param_->rm_avg_update == AVG_CLASSIC && b != frozen_b) {
      double reach_i = range->belief_[i] * pow(10, this_node->reach_adjustment[b]);

      //adjustment adaptively goes up
      if (reach_i > 0.0 && reach_i < 100) {
        pthread_mutex_lock(&this_node->mutex_);
        reach_i = range->belief_[i] * pow(10, this_node->reach_adjustment[b]);
        while (true) {
          if (reach_i > 100)
            break;
          this_node->reach_adjustment[b] += 1;
          reach_i *= 10;
          for (int a = 0; a < a_max; a++) {
            strategy_->ulong_wavg_[rnb0 + a] *= 10;
//            this_node->ulong_wavg_[this_node->HashBa(b, a)] *= 10;
          }
        }
        pthread_mutex_unlock(&this_node->mutex_);
      }

      //adjustment adaptively goes down
      if (reach_i > pow(10, 15)) {
        pthread_mutex_lock(&this_node->mutex_);
        //get the latest value
        reach_i = range->belief_[i] * pow(10, this_node->reach_adjustment[b]);
        while (true) {
          if (reach_i < pow(10, 13)) //goes down two steps
            break;
          this_node->reach_adjustment[b] -= 2;
          reach_i *= 0.01;
          for (int a = 0; a < a_max; a++) {
            strategy_->ulong_wavg_[rnb0 + a] *= 0.01;
          }
        }
        pthread_mutex_unlock(&this_node->mutex_);
      }
      //final update
      reach_i = range->belief_[i] * pow(10, this_node->reach_adjustment[b]);
      if (reach_i > pow(10, 13 + r)) {
        logger::critical(
            "too large!! round %d is reach = %.16f | adjusted = %.16f | reach_adjustment[%d] = %d",
            r,
            range->belief_[i],
            reach_i,
            b,
            this_node->reach_adjustment[b]);
      }

      for (int a = 0; a < a_max; a++) {
        double new_wavg = strategy_->ulong_wavg_[rnb0 + a] + (reach_i * rnb_avg[a]);
        strategy_->ulong_wavg_[rnb0 + a] = (ULONG_WAVG) new_wavg;
      }
    }

    /*
     * range update + pruning
     */
    for (int a = 0; a < a_max; a++) {
      double prob = rnb_avg[a];

      //check pruning if prob = 0, skipping river node and terminal node
      if (iter_prune_flag && prob == 0.0 && !this_node->children[a]->IsTerminal()
          && this_node->children[a]->GetRound() != HOLDEM_ROUND_RIVER) {
//      if (iter_prune_flag && prob == 0.0) {
        auto regret = strategy_->double_regret_[rnb0 + a];
        if (regret <= cfr_param_->rollout_prune_thres) {
          //prune and continue;
          child_ranges[a]->Prune(i);
          continue;
        }
      }

      //zero all super small values < 0.03
      if (prob < RANGE_ROLLOUT_PRUNE_THRESHOLD) {
        child_ranges[a]->Zero(i);
      } else {
        child_ranges[a]->belief_[i] *= prob;
      }

      //safeguarding
      auto new_v = child_ranges[a]->belief_[i];
      if (new_v > 0.0 && new_v < pow(10, -14))
        logger::warn("reach is too small %.16f [r %d]", new_v, this_node->GetRound());
    }
  }
}

/**
 *
 * reminder:: if the next node is the root of a street, you need to handle he value properly. skip all -1
 *
 * @param out_belief
 * @param in_belief_map
 * @param hand_maps
 */
void VectorCfrWorker::ComputeCfu(Node *this_node,
                                 std::vector<sHandBelief *> child_reach_ranges,
                                 std::vector<sHandBelief *> child_cfu,
                                 sHandBelief *cfu,
                                 CFU_COMPUTE_MODE mode,
                                 const float *avg_all) {
  int a_max = this_node->GetAmax();
//  auto r = this_node->GetRound();

  for (auto &i : hand_kernel->valid_index_) {
    //outer pruning || lossless.
    if (cfu->IsPruned(i))
      continue;

//    auto b = hand_kernel->GetBucket(r, i);
    double final_value = 0.0;
    /*
     * compute the final value by mode
     */
    switch (mode) {
      case WEIGHTED_RESPONSE : {
        //multiply with strategy avg value.
        //whether or not the next node is the starting of a street does not matter.
        int offset = i * a_max;
        for (int a = 0; a < a_max; a++) {
          //only do when it is not pruned.
          if (!child_reach_ranges[a]->IsPruned(i)) {
            float weight = avg_all[offset + a];
            if (weight > 0.0)
              final_value += child_cfu[a]->belief_[i] * weight;
          }
        }
        //safe guarding code
        if (final_value > pow(10, 15)) {
          logger::critical("cfr too big %.16f at i = %d > 10^%d", final_value, i, 15);
        }
        //todo if for computing the real cfu at each node, we need to weight it with reach
        break;
      }
      case SUM_RESPONSE : {
        for (int a = 0; a < a_max; a++) {
          //only do when it is not pruned. it should be pruned on the outliar,
          if (!child_reach_ranges[a]->IsPruned(i))
            final_value += child_cfu[a]->belief_[i];
        }
        break;
      }
      case BEST_RESPONSE : {
        final_value = -999999999;
        int offset = i * a_max;
        for (int a = 0; a < a_max; a++) {
          if (avg_all[offset + a] > 0) {
            //otherwise best response will become -1. pruning is never on in best response mode.
            auto util = child_cfu[a]->belief_[i];
            if (util != BELIEF_MASK_VALUE && util > final_value)
              final_value = util;
          }
        }
        //todo: if computing the expl at this node.
        //weighted with weights, need to reweight with 1/ 1000, cuz it scales 1000 both on my reach and opp reach
        //final_value *= reach_ranges->ranges_[my_pos].belief_[i] / 1000.0;
        if (fabs(final_value + 999999999) < 0.001) {
          //it means they are all pruned. probably the next node is the first node of a street. board crashes.
          final_value = BELIEF_MASK_VALUE;
        }
        break;
      }
    }
    cfu->belief_[i] = final_value;
  }
}

void VectorCfrWorker::ConditionalPrune() {
  if (cfr_param_->pruning_on)
    iter_prune_flag = GenRndNumber(1, 100) <= cfr_param_->rollout_prune_prob * 100;
}

void VectorCfrWorker::RegretLearning(Node *this_node, std::vector<sHandBelief *> child_cfu, sHandBelief *cfu) {
  auto a_max = this_node->GetAmax();
  auto r = this_node->GetRound();
  auto n = this_node->GetN();
  auto frozen_b = this_node->frozen_b;

  for (auto &i : hand_kernel->valid_index_) {
    //no need to update with the pruned hand, outer pruning
    if (cfu->IsPruned(i))
      continue;
    auto b = hand_kernel->GetBucket(r, i);
    //if frozen, no need to update regret
    if (b == frozen_b)
      continue;

    auto rnb0 = strategy_->ag_->kernel_->hash_rnba(r, n, b, 0);
    double cfu_i = cfu->belief_[i];
    //update regret
    for (int a = 0; a < a_max; a++) {
      // pruning inside, no need to update regret for it.
      if (child_cfu[a]->IsPruned(i))
        continue;

      double cfu_a = child_cfu[a]->belief_[i];
      double diff = cfu_a - cfu_i; //diff as immediate regret
      //
      double old_reg = strategy_->double_regret_[rnb0 + a];
      double new_reg = ClampRegret(old_reg, diff, cfr_param_->rm_floor);
      if (old_reg > pow(10, 15) || new_reg > pow(10, 15))
        logger::critical(
            "[old reg %.16f] too big![new reg %.16f] [%.16f] [diff %.16f] [u_action %.16f] [cfu %.16f]",
            old_reg,
            new_reg,
            cfr_param_->rm_floor,
            diff,
            cfu_a,
            cfu);
      strategy_->double_regret_[rnb0 + a] = new_reg;
    }
  }
}

double VectorCfrWorker::Solve(Board_t board) {
  auto ag = strategy_->ag_;
  auto starting_round = ag->root_state_.round;
  if (starting_round < 3) {
    delete hand_kernel;
    hand_kernel = new sHandKernel(board, starting_round);
    hand_kernel->EnrichHandKernel(&ag->bucket_reader_);
  } else if (starting_round == 3 && hand_kernel == nullptr) {
    //cache the hand kernel if just solving round RIVER subgame
    hand_kernel = new sHandKernel(board, starting_round);
    hand_kernel->EnrichHandKernel(&ag->bucket_reader_);
  }

  //pruning with prob
  ConditionalPrune();

  //todo: cache this for RIVER subgame.
  sHandBelief local_root_belief[2];
  auto active_players = ag->GetActivePlayerNum();
  for (int p = 0; p < active_players; p++) {
    //prepare
    local_root_belief[p].CopyValue(&ag->root_hand_belief_[p]);
    local_root_belief[p].NormalizeExcludeBoard(board);
  }

  double cfu_sum = 0.0;
  if (mode_ == CFR_VECTOR_PAIRWISE_SOLVE) {
    /*
     * PAIRWISE WALKING
     */
    Ranges starting_ranges{active_players};
    for (int p = 0; p < starting_ranges.num_player_; p++) {
      starting_ranges.beliefs_[p].CopyValue(&local_root_belief[p]);
      starting_ranges.beliefs_[p].Scale(REGRET_SCALER);
    }
    auto cfu = WalkTree_Pairwise(ag->root_node_, &starting_ranges);
    for (int p = 0; p < starting_ranges.num_player_; p++)
      cfu->beliefs_[p].DotMultiply(&local_root_belief[p]);
    cfu_sum = cfu->ValueSum();
//    logger::debug("%f | %f", local_root_belief[0].BeliefSum(), local_root_belief[1].BeliefSum());
    delete cfu;
  } else {
    /*
     * ALTERNATE WALKING
     */
    for (int p = 0; p < active_players; p++) {
      auto opp_belief = local_root_belief[1 - p];
      opp_belief.Scale(REGRET_SCALER);
      sHandBelief *cfu_p = WalkTree_Alternate(ag->root_node_, p, &opp_belief);
      cfu_p->DotMultiply(&local_root_belief[p]);  //illegal parts are 0, so it is fine.
      cfu_sum += cfu_p->BeliefSum();
      delete cfu_p;
    }
  }

  double avg_cfu = cfu_sum / (2 * ag->GetBigBlind());
  return avg_cfu;
}

std::vector<sHandBelief *> VectorCfrWorker::ExtractBeliefs(std::vector<Ranges *> &ranges, int pos) {
  std::vector<sHandBelief *> beliefs;
  int size = ranges.size();
  beliefs.reserve(size);
  for (int a = 0; a < size; a++)
    beliefs.emplace_back(&ranges[a]->beliefs_[pos]);
  return beliefs;
}


