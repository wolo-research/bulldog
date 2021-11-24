//
// Created by Isaac Zhang on 6/10/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_ACTION_CHOOSER_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_ACTION_CHOOSER_HPP_

#include <bulldog/game_util.h>
#include <bulldog/table_util.h>

extern "C" {
#include "bulldog/game.h"
}

struct ACParam {
  double prune_threshold_ = 0.0; //
  double pot_allin_threshold_ =
      0.4; //min allin requirement for all, r4000c/r20000.  it is == 2 pot raise, acceptable
  double fold_threshold_ = 1.0; //max fold requirement to fold
  bool purification_ = false;
};

class ActionChooser {
 public:
  ACParam params_;

  void ChooseAction(float *rnb_avg, int a_max, Node *matched_node, Action &r_action) const {
    r_action.size = 0;

    //do sanity check
    float sum_avg = 0.0;
    for (auto a = 0; a < a_max; a++)
      sum_avg += rnb_avg[a];
    //if it happens, it may be a error in training
    if (sum_avg == 0) {
      logger::warn("    [ACTION CHOOSER] : sum avg of valid actions is zero, returning fold");
      r_action.type = a_fold;
      return;
    }

    Action first_action = matched_node->children[0]->GetLastAction();
    int al_fold = 0, al_call = 0, al_raise = 0;
    if (first_action.type == a_fold) {
      al_fold = a_fold;
      al_call = a_call;
      al_raise = a_raise;
    } else if (first_action.type == a_call) {
      al_fold = -1;
      al_call = 0;
      al_raise = 1;
    } else {
      logger::critical("first action unrecognized %d:%d", first_action.type, first_action.size);
    }

    //if fold exceeds threshold, we fold with probability 1
    if (al_fold == a_fold && rnb_avg[al_fold] >= params_.fold_threshold_) {
      r_action.type = a_fold;
      logger::debug("    [ACTION CHOOSER] :    fold action [%c%d] chosen with probability [%f]",
                    actionChars[r_action.type],
                    r_action.size,
                    rnb_avg[al_fold]);
      return;
    }

    if (params_.purification_) {
      float raise_total = 0.0;
      for (int a = al_raise; a < a_max; a++) {
        raise_total += rnb_avg[a];
      }

      if (al_fold == a_fold) {
        //first action is a fold
        if (rnb_avg[al_call] > rnb_avg[al_fold] && rnb_avg[al_call] > raise_total) {
          //return call action
          r_action.type = a_call;
          logger::debug("    [ACTION CHOOSER] :      purified action [%c%d] chosen with probability [%f]",
                        actionChars[r_action.type],
                        r_action.size,
                        rnb_avg[al_call]);
          return;
        } else if (rnb_avg[al_fold] > rnb_avg[al_call] && rnb_avg[al_fold] > raise_total) {
          //return fold action
          r_action.type = a_fold;
          logger::debug("    [ACTION CHOOSER] :      purified action [%c%d] chosen with probability [%f]",
                        actionChars[r_action.type],
                        r_action.size,
                        rnb_avg[al_fold]);
          return;
        }
        //otherwise proceed below to purified raise actions
      } else if (rnb_avg[al_call] > raise_total) {
        //first action is a call and sum prob > raise total
        r_action.type = a_call;
        logger::debug("    [ACTION CHOOSER] :      purified action [%c%d] chosen with probability [%f]",
                      actionChars[r_action.type],
                      r_action.size,
                      rnb_avg[al_call]);
        return;
      }
      //meta-action bet is greater
      //perform purification again
      float max_value = -1;
      float chosen_prob = 0.0;
      for (int a = al_raise; a < a_max; a++) {
        if (rnb_avg[a] > max_value) {
          max_value = rnb_avg[a];
          auto matched_max = matched_node->children[a]->GetLastAction();
          r_action.size = matched_max.size;
          r_action.type = matched_max.type;
          chosen_prob = rnb_avg[a];
        }
      }
      logger::debug("    [ACTION CHOOSER] :      purified action [%c%d] chosen with probability [%f]",
                    actionChars[r_action.type],
                    r_action.size,
                    chosen_prob);
      return;
    }

    //only if u have more than 3 actions available (e.g. f, c, a)
    if (a_max > 3) {
      int final_action_idx = a_max - 1;
      if (rnb_avg[final_action_idx] != 0.0) {
        //check if the action == all in
        auto last_action = matched_node->children[final_action_idx]->GetLastAction();
        //the all-in is the same
        if (ActionToTableAction(matched_node->game_, matched_node->state_, last_action)
            == TABLE_ACTION_ALLIN_RAISE) {
          //compare the total pot
          auto sum_pot = GetTotalPot(matched_node->state_);
          if (sum_pot < last_action.size * params_.pot_allin_threshold_) {
            logger::debug("    [ACTION CHOOSER] : all-in %d is too much for sum_pot %d. force remove it then renorm", actionToCode(&last_action), sum_pot);
            if (rnb_avg[final_action_idx] > 0.9) {
              logger::warn("    [ACTION CHOOSER] : all-in at [round %d] is too much. force it to the next largest bet", matched_node->GetRound());
              rnb_avg[final_action_idx -1] = rnb_avg[final_action_idx];
            }
            rnb_avg[final_action_idx] = 0.0;
            NormalizePolicy(rnb_avg, a_max);
          }
        }
      }
    }

    //prune before probabilistically selecting
    bool normalized_for_pruning = false;
    for (auto a = 0; a < a_max; a++) {
      if (rnb_avg[a] == 0.0)
        continue;
      if (rnb_avg[a] <= params_.prune_threshold_) {
        logger::debug("    [ACTION CHOOSER] :    prune [action %d] [%f < %f]",
                      matched_node->children[a]->GetLastActionCode(),
                      rnb_avg[a],
                      params_.prune_threshold_);
        rnb_avg[a] = 0.0;
        normalized_for_pruning = true;
      }
    }
    if (normalized_for_pruning) {
      NormalizePolicy(rnb_avg, a_max);
    }

    //pick uniformly from avg[]
    int picked_number = RandomPick(rnb_avg, a_max);
    auto matched_action = matched_node->children[picked_number]->GetLastAction();
    r_action.size = matched_action.size;
    r_action.type = matched_action.type;
    logger::debug("    [ACTION CHOOSER] :      standard action [%c%d] chosen with probability [%f]",
                  actionChars[r_action.type],
                  r_action.size,
                  rnb_avg[picked_number]);
  }

  void ConfFromJson(web::json::value action_conf) {
    params_.purification_ = action_conf.at("purification").as_bool();
    params_.fold_threshold_ = action_conf.at("fold_threshold").as_double();
    params_.pot_allin_threshold_ = action_conf.at("pot_allin_threshold").as_double();
    params_.prune_threshold_ = action_conf.at("prune_threshold").as_double();
  }
};

#endif //BULLDOG_MODULES_ENGINE_SRC_ACTION_CHOOSER_HPP_
