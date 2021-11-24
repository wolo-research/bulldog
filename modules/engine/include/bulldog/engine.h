//
// Created by Isaac Zhang on 2/25/20.
//

/**
 * overall design of the engine
 * - blind-invariant, stack-aware
 * - nested {hand, state-action} loop
 * - one engine, one game def (e.g. NLH2), one session
 *
 * strict lifecycle
 *  -
 */

#ifndef BULLDOG_MODULES_ENGINE_SRC_ENGINE_H_
#define BULLDOG_MODULES_ENGINE_SRC_ENGINE_H_

#include "../../src/cfr.h"
#include "../../src/strategy.h"
#include "../../src/strategy_io.h"
#include "../../src/subgame_solver.hpp"
#include "../../src/action_chooser.hpp"
#include "../../src/strategy_pool.hpp"

#include <string>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <future>

extern "C" {
#include "bulldog/game.h"
}

const int GAME_TYPE_NOT_COMPATIBLE = 0;

const int NEW_SESSION_FAILURE = 10;
const int NEW_SESSION_SUCCESS = 11;
const int END_SESSION_SUCCESS = 12;

const int NEW_HAND_SUCCESS = 20;

const int MATCH_STATE_PARSING_FAILURE = 30;
const int MATCH_STATE_PARSING_SUCCESS = 32;
const int GET_ACTION_SUCCESS = 31;
const int GET_ACTION_FAILURE = 33;

static std::map<int, std::string> ENGINE_CODE_TO_NAME = {
    {GET_ACTION_FAILURE, "GET_ACTION_FAILURE"}
};

struct TableContext {
  Game session_game;
  std::string table_name_;
};

struct PlayBook {
  PlayBook(Strategy *strategy,
           ActionChooser *default_action_chooser,
           STRATEGY_TYPE playing_strategy)
      : strategy_(strategy),
        action_chooser_(default_action_chooser),
        playing_strategy_(playing_strategy) {}
  Strategy *strategy_ = nullptr;
  ActionChooser *action_chooser_ = nullptr;
  STRATEGY_TYPE playing_strategy_;
};

class Engine {
 public:
  std::string engine_name_;

  Engine(const char *engine_conf_file, Game *game);
  Engine(const char *engine_conf_file, Game *game, BucketPool *bucket_pool, StrategyPool* blueprint_pool_);
  ~Engine();
  std::string GetName() const;
  Game *GetGame();
  int SetTableContext(const TableContext &table_context);
  int RefreshEngineState();
  int GetAction(MatchState *new_match_state, Action &r_action, double timeout_ms);
  int GetActionBySession(MatchState &normalized_match_state, Action &r_action, int timeout);
  void EvalShowdown(MatchState &match_state);
  int TranslateToNormState(const std::string &match_state_str, MatchState &normalized_match_state);
  TableContext table_context_;
 private:
  /*
   * META, by configuration
   */
  Game *normalized_game_;
  BucketPool *bucket_pool_ = nullptr;
  StrategyPool* blueprint_pool_ = nullptr;
  ActionChooser* default_action_chooser_ = nullptr;
  //todo: why not use std::vector?
  SubgameSolver *subgame_solvers_ = nullptr;
  int sgs_size_ = 0;
  bool is_daemon_engine = false;
  bool random_action_ = false;
  bool busy_flag_;
  //
  bool owning_pool_ = false;

  /*
   * HAND SESSION STATE
   */
  MatchState last_matchstate_;
  std::vector<PlayBook> playbook_stack_;
  std::vector<Strategy *> sgs_strategy_stack_;

  std::atomic_bool sgs_cancel_token_;
  std::atomic_bool daemon_cancel_token_;

  void AsynStartDaemonSolving(SubgameSolver *sgs, int checkpoint);
  void AsynStopDaemonSolving();
  void AsynStopCFRSolving();
  int AsynStartCFRSolving(SubgameSolver *selected_sgs, Strategy *&new_strategy, double remaining_ms);
  bool InputSanityCheck(MatchState *new_match_state);
  bool EngineStateStaleCheck(MatchState *new_match_state);
  //helper functions
  bool IsNestedSgsStarted();
  int GetRandomAction(MatchState *new_match_state, Action &r_action);
  static bool ValidatePlaybook(PlayBook &playbook, MatchState *new_match_state, int subgame_built_code);

};

#endif //BULLDOG_MODULES_ENGINE_SRC_ENGINE_H_
