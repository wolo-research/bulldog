//
// Created by Isaac Zhang on 3/10/20.
//

#include <spdlog/spdlog.h>
#include <bulldog/logger.hpp>
#include <bulldog/game_util.h>
#include "action_abs.h"

/*
 * the last_action is random and does not matter for a empty state.
 */
Node *CompositeActionAbs::BuildBettingTreeInner(Game *game,
                                                int root_round,
                                                State current_state,
                                                Node_t *choice_node_index_by_round,
                                                Node_t *term_node_index_by_round,
                                                Node *parent_node,
                                                State *forced_state,
                                                bool depth_limited) {
  Node *new_node;
  new_node = new Node(game, parent_node);
  new_node->SetState(current_state);

  /*
   * terminal node
   */
  uint8_t round = current_state.round;
  if (current_state.finished) {
    //no strategy slot needed for terminal nodes
    new_node->SetIndex(term_node_index_by_round[round]++);
#if DEV > 5
    new_node->PrintState("TERMINAL NODE -- ");
#endif
    return new_node;
  }
  /*
   * optionally, it can be a leaf node
   * just a flag.
   */
  if (depth_limited) {
    if (new_node->FollowingChanceNode() && new_node->GetRound() > root_round) {
      new_node->is_leaf_node = true;
      return new_node;
    }
  }

  /*
   * choice node
   */
  new_node->SetIndex(choice_node_index_by_round[round]++);
  //a copy of action vector
  std::vector<Action> candidate_actions = GetCompositeActions(game, current_state);

  //match if the state of this node, is a part of forced state
  if (forced_state != nullptr && IsPriorStateSameRound(&new_node->state_, forced_state)) {
    auto new_node_kth_action = new_node->state_.numActions[round];
    Action forced_action = forced_state->action[round][new_node_kth_action]; // the next action
    //make sure no action are the same as forced_action
    bool crash = false;
    for (auto &c : candidate_actions)
      if (c.type == forced_action.type && c.size == forced_action.size)
        crash = true;

    //forcely add it to the actionm
    if (!crash) {
      // delete similar size raise action (< 100)
      if (forced_action.type == a_raise)
        for (auto it = candidate_actions.begin(); it != candidate_actions.end();) {
          if (abs(it->size - forced_action.size) < 100)
            it = candidate_actions.erase(it);
          else
            ++it;
        }
      candidate_actions.emplace_back(forced_action);
      logger::debug("forced action %d is appended to the following state", actionToCode(&forced_action));
      new_node->PrintState("appended state");
    } else {
      logger::trace("the forced action %d is forfeited because it already exists", actionToCode(&forced_action));
    }
  }

  /*
   * sort actions and remove duplicated actions
   */
  std::sort(candidate_actions.begin(), candidate_actions.end(), [](Action &a, Action &b) {
    return actionToCode(&a) < actionToCode(&b);
  });

  int last_code = -2;
  for (auto it(candidate_actions.begin()); it != candidate_actions.end();) {
    int new_code = actionToCode(&*it);
    if (new_code == last_code) {
#if DEV > 1
      logger::debug("remove duplicated action %d in the following state", last_code);
      new_node->PrintState();
#endif
      it = candidate_actions.erase(it);
    } else {
      last_code = new_code;
      ++it;
    }
  }
#if DEV > 5
  new_node->PrintState("  CHOICE NODE -- ");
#endif

  /*
   * recurse down and build child node
   */
  for (auto &action : candidate_actions) {
    //genenerate next node, but see if the round or the game has ended. sample chance and terminal node respectively.
    State new_state(current_state);
    doAction(game, &action, &new_state);
    Node *child_node = BuildBettingTreeInner(game, root_round,
                                             new_state,
                                             choice_node_index_by_round,
                                             term_node_index_by_round,
                                             new_node,
                                             forced_state,
                                             depth_limited);
    new_node->children.emplace_back(child_node);
  }
  new_node->SortChildNodes();
  return new_node;
}

Node *CompositeActionAbs::BuildBettingTree(Game *game, State current_state, State *forced_state, bool depth_limited) {
  uint32_t choice_node_index_by_round[4]{0, 0, 0, 0};
  uint32_t term_node_index_by_round[4]{0, 0, 0, 0};
  //hack to prevent undefined printCard behavior
//  current_state.holeCards[0][0] = 0;
//  current_state.holeCards[0][1] = 1;
//  current_state.holeCards[1][0] = 2;
//  current_state.holeCards[1][1] = 3;

  //the root note has a random generate last_action. But it is not important, as we wont use it.  a hack
  auto root_node = BuildBettingTreeInner(game,
                                         current_state.round,
                                         current_state,
                                         choice_node_index_by_round,
                                         term_node_index_by_round,
                                         nullptr,
                                         forced_state, depth_limited);
  logger::require_critical(IsBettingTreeValid(root_node),
                           "Invalid Betting Tree!!!");
  return root_node;
}

bool CompositeActionAbs::IsBettingTreeValid(Node *this_node) {
  if (this_node->IsTerminal())
    return true;
  if (this_node->IsLeafNode())
    return true;
  /*
   * have children if it is a choicen node
   */
  auto child_size = this_node->children.size();
  if (child_size == 1) {
    logger::warn("only one child for this node");
    this_node->PrintState();
  }
  if (child_size == 0) {
    logger::warn("no children for the node");
    this_node->PrintState();
    return false;
  }

  /*
   * check duplicated or orsorted
   */
  int last_code = -2;
  for (auto c : this_node->children) {
    int new_code = c->GetLastActionCode();
    if (new_code <= last_code) {
      logger::error("got duplicated or unsorted action %d and %d", last_code, new_code);
      return false;
    }
    last_code = new_code;
  }

  /*
   * should always have call
   */
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

  bool valid = true;
  for (auto c : this_node->children) {
    valid = valid && IsBettingTreeValid(c);
  }
  return valid;
}

std::vector<Action> Fcpa::GetCandidateActions(Game *game, State state) {
  std::vector<Action> valid_actions;
  auto current_round = state.round;
  uint8_t acting_player = currentPlayer(game, &state);
  /*
   *   3 action types, fold 0, call 1, raise 2.
   *   sequence matters. as we need to have a flag to elimimate duplicate action where call = all_in.
   */

  for (int a = 0; a < NUM_ACTION_TYPES; ++a) {
    Action action;
    action.type = (ActionType) a;
    action.size = 0;

    if (action.type != a_raise) {
      /*
       * theorectically, fold and call are always valid
       * however, sometimes fold is not sensible, if you are already all in, or have spent as maxspent of the round.
       * in this case, fold is not valid. we delete all premature fold
       */
      auto code = isValidAction(game, &state, 0, &action);
      //premature fold == 5
      if (code == 1) {
        valid_actions.emplace_back(action);
      }
    } else {
      /*
       * perform raise action calculation. if possible
       */

      int32_t min_raise_size;
      int32_t max_raise_size;
      auto raise_valid_flag = raiseIsValid(game, &state, &min_raise_size, &max_raise_size);
      if (raise_valid_flag) {
        /*
         * at any given decision point, the actor can do an array of raise actions with respect to some pot method
         *  - POT_NET: simple
         *  - POT_AFTER_RAISE: as how pot limit ohama does it
         *  - LAST_BET: if none raise before, then it is equivalent to POT_NET, or it catches the last raise action
         *
         *  (a) get the raise base for multipling
         *  (b) get the raise action multiplier array
         *  (c) compute the desired raise size, and adjust to [min, max] before return
         */

        /*
         * (a)
         */
        auto mode = raise_mode_;
        //if no previous raise, then we transform the mode
        if (mode == NET_OR_X) {
          mode = WithEarlyRaiseAction(&state) ? X_LAST_RAISE : POT_NET;
        }
        int raise_base = GetRaiseBase(game, &state, mode);
        if (raise_base == -1)
          logger::critical("sth wrong with pot calculation");

        /*
         * (b)
         */
        std::vector<double> candidate_raise_ratio;
        //now we are constrained by the configured action depth
        //index starts from 0. so numAction is the index of new action.
        auto numActions = state.numActions[current_round];
        for (auto &raise_conf : raise_config_) {
          if (raise_conf.IsApplicable(numActions, mode)) {
            candidate_raise_ratio.emplace_back(raise_conf.ratio);
          }
        }
        /*
         * (c)
         * - (c1) get the fianl raise size
         * - (c2) adjust to the legal range
         */
        for (auto raise_multiplier : candidate_raise_ratio) {
          if (raise_multiplier == -1) {
            /* Now add all-in */
            action.size = max_raise_size;
            valid_actions.push_back(action);
          } else {
            /*
             * normal raise actions.
             * per ACPC protocol: raise = spent + newly_added
             */
            int32_t final_raise_size = 0;
            switch (mode) {
              case POT_NET:
              case POT_AFTER_CALL: {
                int32_t already_spent = state.spent[acting_player];
                //need to += amount fo call if in after_call mode
                if (mode == POT_AFTER_CALL)
                  already_spent += state.maxSpent - state.spent[acting_player];
                final_raise_size = raise_multiplier * raise_base + already_spent;
                break;
              }
              case X_LAST_RAISE: {
                int r = state.round;
                int32_t
                    already_spent = r == 0 ? 0 : state.sum_round_spent[current_round - 1][acting_player];
                final_raise_size = already_spent + raise_base * raise_multiplier;
                break;
              }
              case NET_OR_X:logger::critical("dont use net_or_x here. it is composite type");
                break;
            }
            /*
             * (c2) do final checking, clamp to [min, max] and remove if just 10% less than max.
             */
            if (final_raise_size >= max_raise_size) {
              action.size = max_raise_size;
              valid_actions.push_back(action);
              break; //should not just continue, but break it, because the raise is already all-in.
            }
            if (final_raise_size < min_raise_size) {
              final_raise_size = min_raise_size;
            }
            //remove actions like 18000 and 20000, 10% threshold
            double final_to_max = (double) final_raise_size / (double) max_raise_size;
            if (final_to_max >= high_filter_)
              continue;

            //just add it to abstraction.
            action.size = final_raise_size;
            valid_actions.push_back(action);
          }
        }
      }
    }
  }
  return valid_actions;
}
