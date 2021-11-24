//
// Created by Isaac Zhang on 3/10/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_ACTION_ABS_H_
#define BULLDOG_MODULES_ENGINE_SRC_ACTION_ABS_H_

#include <cpprest/json.h>
#include <utility>
#include <bulldog/card_util.h>
#include "node.h"
extern "C" {
#include "bulldog/game.h"
}

static std::map<std::string, RAISE_MODE> PotEstimateMap{
    {"net", POT_NET},
    {"after_call", POT_AFTER_CALL},
    {"net_or_x", NET_OR_X},
    {"x", X_LAST_RAISE},
};

struct RaiseConfig {
  RAISE_MODE raise_mode_;
  double ratio;
  //index are incluisve
  int begin_idx;
  int end_idx;
  bool IsApplicable(uint8_t num_action, RAISE_MODE mode) {
    return mode == raise_mode_ && num_action >= begin_idx && num_action <= end_idx;
  }
};

class Fcpa {
 public:
  Fcpa() = default;
  void Configure(web::json::value config) {
    logger::require_critical(config.has_field("version"), "must have version in actions_abs");
    auto version_string = config.at("version").as_string();
    logger::require_critical(version_string == version_, "action_abs version wrong conf = %s | code = %s", version_string, version_);
    logger::require_critical(config.has_field("raise_mode"), "must have raise_mode in actions_abs");
    raise_mode_ = PotEstimateMap[config.at("raise_mode").as_string()];

    if (config.has_field("high_filter"))
      high_filter_ = config.at("high_filter").as_double();
    auto raise_configs = config.at("raise_config").as_array();
    for (auto r_config : raise_configs) {
      raise_config_.push_back(RaiseConfig{
          PotEstimateMap[r_config.at("mode").as_string()],
          r_config.at("ratio").as_double(),
          r_config.at("begin").as_integer(),
          r_config.at("end").as_integer(),
      });
    }
  }
  std::string version_ = "v1";
  RAISE_MODE raise_mode_;
  std::vector<RaiseConfig> raise_config_;
  std::vector<Action> GetCandidateActions(Game *game, State state);
  double high_filter_ = 0.85;
};

class CompositeActionAbs {
 public:
  CompositeActionAbs() = default;
  explicit CompositeActionAbs(web::json::value &config) {
    auto all_config = config.as_array();
    if (all_config.size() != HOLDEM_MAX_ROUNDS) {
      logger::critical("must have 4 configuration files for holdem");
    }
    for (int r = 0; r < HOLDEM_MAX_ROUNDS; r++) {
      workers[r].Configure(all_config[r]);
    }
  }

  Node *BuildBettingTree(Game *game, State current_state, State *forced_state = nullptr, bool depth_limited = false);
  Node *BuildBettingTreeInner(Game *game,
                              int root_round,
                              State current_state,
                              Node_t *choice_node_index_by_round,
                              Node_t *term_node_index_by_round,
                              Node *parent_node,
                              State *forced_state,
                              bool depth_limited);
  static bool IsBettingTreeValid(Node *this_node);
  std::vector<Action> GetCompositeActions(Game *game, State state) {
    auto round = state.round;
    return workers[round].GetCandidateActions(game, state);
  }
  std::array<Fcpa, 4> workers;
};
#endif //BULLDOG_MODULES_ENGINE_SRC_ACTION_ABS_H_
