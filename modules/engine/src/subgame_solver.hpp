//
// Created by Isaac Zhang on 5/25/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_SUBGAME_SOLVER_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_SUBGAME_SOLVER_HPP_

#include "strategy.h"
#include "ag_builder.hpp"
#include "cfr.h"
#include "cfr_state.h"

enum SUBGAME_BUILT_CODE {
  UNSUPPORTED_SUBGAME,
  SOLVE_ON_NEW_ROUND_HERO_FIRST,
  SOLVE_ON_NEW_ROUND_OPPO_FIRST,
  SKIP_RESOLVING_SUBGAME,
  RESOLVING_NONE_PRIOR_ACTION,
  RESOLVING_WITH_PRIOR_ACTION
};

inline std::map<int, std::string> SubgameBuiltCodeMap{
    {SOLVE_ON_NEW_ROUND_HERO_FIRST,"SOLVE_ON_NEW_ROUND_HERO_FIRST"},
    {SOLVE_ON_NEW_ROUND_OPPO_FIRST,"SOLVE_ON_NEW_ROUND_OPPO_FIRST"},
    {SKIP_RESOLVING_SUBGAME,"SKIP_RESOLVING_SUBGAME"},
    {RESOLVING_NONE_PRIOR_ACTION,"RESOLVING_NONE_PRIOR_ACTION"},
    {RESOLVING_WITH_PRIOR_ACTION, "RESOLVING_WITH_PRIOR_ACTION"}
};

struct SubgameSolver {
  virtual ~SubgameSolver() {
    delete cfr_;
    delete ag_builder_;
    delete convergence_state_;
    delete action_chooser_;
  }

  SubgameSolver() {
    convergence_state_ = new sCFRState();
    action_chooser_ = new ActionChooser();
  }

  std::string name_ = "default_sgs";

  //outer triggers:  default to be matching by ronud, unless distances are specified
  int active_round = -1;
  //only use in bigpot solver for now
  double active_offtree_min = 9999999;
  int active_bet_seq_min = 999999; //default not using.
  int active_sumpot_min = 9999999; //

  //inner triggers:
  double resolve_offtree_min = 9999999;
  int resolve_sumpot_min = 9999999; //default to active it.
  int resolve_last_root_sumpot_min = 1000000000; //default not stepping back

  //actors
  AGBuilder *ag_builder_ = nullptr;
  CFR *cfr_ = nullptr;
  sCFRState *convergence_state_;
  ActionChooser *action_chooser_;
  STRATEGY_TYPE playing_strategy_ = STRATEGY_REG; //default at reg

  /*
   * water fall detection
   *  - first check if pushable
   *  - then pot
   *  - then bet_seq
   *  - then off_tree
   */
  bool CheckTriggerCondition(NodeMatchCondition &condition) {
    if (condition.matched_node_->GetRound() == active_round) {
      if (condition.matched_node_->GetSumPot() >= active_sumpot_min) {
        logger::debug("    [SGS %s] : triggers by sumpot %d", name_, condition.matched_node_->GetSumPot());
        return true;

      } else if (condition.bet_sim_dist_ >= active_bet_seq_min) {
        logger::debug("    [SGS %s] : triggers by bet sequence pattern distance %d", name_, condition.bet_sim_dist_);
        return true;

      } else if (condition.off_tree_dist_ >= active_offtree_min) {
        logger::debug("    [SGS %s] : triggers by offtree distance %f", name_, condition.off_tree_dist_);
        return true;
      }
    }
    logger::debug("    [SGS %s] : not triggered.", name_);
    return false;
  }

/*
 * how to build the subgame defines the subgame solver
 * we follow the design of pluribus impl (unsafe, but always solve from the street root)
 *  - to force the previous unseen action into the abstraction
 *  - to force the strategy of already happened actions of mine frozen
 * specifically,
 * - CheckNewRound()
 * - resolving if needed
 *  - kth_action = 1, do step back, it is equivalent to the street root
 *  - > 1, step back to the root.
 *
 */
  int BuildSubgame(AbstractGame *ag,
                   Strategy *last_strategy,
                   NodeMatchCondition &condition,
                   MatchState *real_match_state) {
    //never resolve back to the preflop.
    State &real_state = real_match_state->state;
    auto r = real_state.round;
    if (r == HOLDEM_ROUND_PREFLOP) {
      logger::warn("    [SGS %s] : subgame solving at preflop is not yet supported. build subgame fails", name_);
      return UNSUPPORTED_SUBGAME;
    }

    /*
     * CHECK NEW ROUND
     *  where the actor has taken none actions
     *
     *  in 2p game, it can be kth_action = 0 | 1
     */
    auto kth_action = real_state.numActions[r];
    //hero act first _ new round
    if (kth_action == 0) {
      logger::debug("    [SGS %s] : built subgame [step back 0] for new round for [r = %d] [kth_action = %d]", name_, r,
                    kth_action);
      ag_builder_->Build(ag, &real_state);
      return SOLVE_ON_NEW_ROUND_HERO_FIRST;
    }

    //opp act first _ new round/ or resolving
    if (kth_action == 1) {
      if (!BuildResolvingSubgame(ag, real_match_state, 1)) {
        return SKIP_RESOLVING_SUBGAME;
      }
      return SOLVE_ON_NEW_ROUND_OPPO_FIRST;
    }

    /*
     * check if we should resolve by
     * - off_tree trigger
     * - pot limit trigger?
     */
    if (condition.off_tree_dist_ >= resolve_offtree_min) {
      logger::debug("    [SGS %s] : subgame resolving activated by off_tree_dist", name_);
    } else if ((real_state.spent[0] + real_state.spent[1]) >= resolve_sumpot_min) {
      logger::debug("    [SGS %s] : subgame resolving activated by min_pot", name_);
    } else {
      logger::debug("    [SGS %s] : skip resolving subgame. resolving requirement not met", name_);
      return SKIP_RESOLVING_SUBGAME;
    }

    /*
     * do resolving, firstly decide how many steps to take back
     * - if last strategy is at the prior round, resolve to street root. todo: not very likely to happen
     * - if last startegy is at the same round, resolve to the root of last strategy.
     */
    auto this_round = real_match_state->state.round;
    auto last_root_pot = last_strategy->ag_->root_state_.spent[0] + last_strategy->ag_->root_state_.spent[0];
    // last strategy is at the same round && sumpot_min requirements meets
    bool step_back_to_last_root =
        last_strategy->ag_->root_node_->GetRound() == this_round
        && last_root_pot >= resolve_last_root_sumpot_min;

    int steps_to_reverse;
    if (!step_back_to_last_root) {
      logger::debug("    [SGS %s] : skipping resolve to street root cuz %d < %d", name_, last_root_pot,
                    resolve_last_root_sumpot_min);
      steps_to_reverse = 1;
    } else {
      uint8_t step_to_last_root = kth_action - last_strategy->ag_->root_state_.numActions[this_round];
      steps_to_reverse = step_to_last_root;
    }
//    logger::debug("    [SGS %s] : resolving takes [step back %d]", name_, steps_to_reverse);
    if (!BuildResolvingSubgame(ag, real_match_state, steps_to_reverse)) {
      return SKIP_RESOLVING_SUBGAME;
    }
    ag->root_node_->PrintState("    new subgame root : ");
    return (steps_to_reverse > 1) ? RESOLVING_WITH_PRIOR_ACTION : RESOLVING_NONE_PRIOR_ACTION;
  }

  bool BuildResolvingSubgame(AbstractGame *ag, MatchState *real_match_state, int steps_to_reverse) {
    State &real_state = real_match_state->state;
    //build with a step back
    State *step_back_state = new State;
    if (StepBackAction(ag_builder_->game_, &real_state, step_back_state, steps_to_reverse) == -1) {
      logger::warn("    [SGS %s] : stepping too many steps. u may have an empty state or invalid state. return false",
                   name_);
      return false;
    };
    if (real_state.round != step_back_state->round) {
      logger::error("    [SGS %s] : can not step back to the last round", name_);
      return false;
    }
    ag_builder_->Build(ag, step_back_state, &real_state, cfr_->cfr_param_.depth_limited);
    delete step_back_state;
    logger::debug("    [SGS %s] : built subgame [step back %d] for r = %d", name_, steps_to_reverse, real_state.round);
    return true;
  }

  void ConfigWithJson(const char *config_file, BucketPool *bucket_pool) {
    std::filesystem::path dir(BULLDOG_DIR_CFG_ENG);
    std::filesystem::path filename(config_file);
    std::ifstream sgs_file(dir / filename);
    web::json::value sgs_conf;
    if (sgs_file.good()) {
      std::stringstream buffer;
      buffer << sgs_file.rdbuf();
      sgs_conf = web::json::value::parse(buffer);
    }

    logger::require_critical(sgs_conf.has_field("builder_file"), "please fill in build_file || sgs conf");
    logger::require_critical(sgs_conf.has_field("cfr_file"), "please fill in cfr_file || sgs conf");
    logger::require_critical(sgs_conf.has_field("trigger"), "please fill in trigger || sgs conf");
    auto sgs_conf_str = std::string(config_file);
    name_ = sgs_conf_str.substr(0, sgs_conf_str.length() - 5);//get ride of
    if (sgs_conf.has_field("action_chooser")) {
      auto action_conf = sgs_conf.at("action_chooser");
      action_chooser_->ConfFromJson(action_conf);
    }
#if 0
    //make convergence self enclosed in the cfr
    auto conv_config = sgs_conf.at("convergence");
    convergence_state_->iteration =
        conv_config.has_field("iter") ? conv_config.at("iter").as_integer() : -1;
    convergence_state_->time_milli_seconds =
        conv_config.has_field("time_ms") ? conv_config.at("time_ms").as_double() : -1;
    convergence_state_->exploitability =
        conv_config.has_field("expl") ? conv_config.at("expl").as_double() : -1;
    convergence_state_->expl_std =
        conv_config.has_field("expl_std") ? conv_config.at("expl_std").as_double() : -1;
#endif
    cfr_ = new CFR(sgs_conf.at("cfr_file").as_string().c_str());
    convergence_state_->iteration = cfr_->cfr_param_.iteration;
    cfr_->BuildCMDPipeline();
    cfr_->profiling_writer_.prefix_ = cfr_->cfr_param_.name + "_" + std::to_string(convergence_state_->iteration);

    //set playing strategy
    if (sgs_conf.has_field("playing_strategy"))
      playing_strategy_ = StrategyMap[sgs_conf.at("playing_strategy").as_string()];

    std::filesystem::path file(sgs_conf.at("builder_file").as_string());
    ag_builder_ = new AGBuilder((dir / file), bucket_pool);

    auto sgs_trigger = sgs_conf.at("trigger");
    //must have
    logger::require_warn(sgs_trigger.has_field("active_round"), "must specify round for sgs", nullptr);
    active_round = sgs_trigger.at("active_round").as_integer();

    //optional
    //outer
    if (sgs_trigger.has_field("active_offtree_min"))
      active_offtree_min = sgs_trigger.at("active_offtree_min").as_double();
    if (sgs_trigger.has_field("active_bet_seq_min"))
      active_bet_seq_min = sgs_trigger.at("active_bet_seq_min").as_integer();
    if (sgs_trigger.has_field("active_sumpot_min"))
      active_sumpot_min = sgs_trigger.at("active_sumpot_min").as_integer();

    //inner trigger
    if (sgs_trigger.has_field("resolve_offtree_min"))
      resolve_offtree_min = sgs_trigger.at("resolve_offtree_min").as_double();
    if (sgs_trigger.has_field("resolve_sumpot_min"))
      resolve_sumpot_min = sgs_trigger.at("resolve_sumpot_min").as_integer();
    if (sgs_trigger.has_field("resolve_last_root_sumpot_min")) {
      resolve_last_root_sumpot_min = sgs_trigger.at("resolve_last_root_sumpot_min").as_integer();
//      if (resolve_last_root_sumpot_min > active_sumpot_min) {
//        logger::error_and_exit(
//            "resolve_last_root_sumpot_min %d must <= active_sumpot_min %d. "
//            "you may solve in middle and then try to resolve it from root again", resolve_last_root_sumpot_min, active_sumpot_min);
//      }
    }
  }
};

#endif //BULLDOG_MODULES_ENGINE_SRC_SUBGAME_SOLVER_HPP_
