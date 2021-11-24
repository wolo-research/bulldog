//
// Created by Isaac Zhang on 4/9/20.
//
#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>
#include <bulldog/logger.hpp>
#include "../src/action_abs.h"
#include "../src/rnba_kernel.hpp"
#include "../src/abstract_game.h"
#include "../src/strategy.h"

extern "C" {
#include <bulldog/game.h>
};

#include <filesystem>

using namespace std;
TEST_CASE("rnba kernel", "[engine]") {
  logger::init_logger("trace");
  CompositeActionAbs act_abs{};
  act_abs.workers[0].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[0].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[0].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[1].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[1].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[1].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[2].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[2].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[2].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[3].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[3].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[3].raise_mode_ = POT_AFTER_CALL;
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path game_file("holdem.nolimit.2p.game");
  FILE *file = fopen((dir / game_file).c_str(), "r");
  if (file == nullptr) {
    logger::error("failed to open game file %s", game_file);
    exit(-1);
  }
  Game *game_ = readGame(file);
  State state;
  initState(game_, 0, &state);
  auto root_node_ = act_abs.BuildBettingTree(game_, state, nullptr, false);
  uint32_t bucket_count_by_round[4]{169, 100, 100, 100};
  sRNBAKernel kernel_;
  kernel_.BuildInternal(root_node_, bucket_count_by_round, false);
  kernel_.Print();
  /*
   * get all possible rnba, find all of them have distinct value;
   */
  RNBA max_lean = -1;
  for (Round_t r = 0; r < 4; r++) {
    auto n_max = kernel_.nmax_by_r_[r];
    for (Node_t n = 0; n < n_max; n++) {
      for (Bucket_t b = 0; b < bucket_count_by_round[r]; b++) {
        auto a_max = kernel_.amax_by_rn_[r][n];
        for (Action_t a = 0; a < a_max; a++) {
          auto lean = kernel_.hash_rnba(r, n, b, a); //default is lean, and with overflown check
          bool lb_less = lean == (max_lean + 1);
          if (!lb_less) {
            logger::debug("problem at r = %d | n = %d | b = %d | a =%d", r, n, b, a);
          }
          REQUIRE(lb_less);
          max_lean++;
        }
      }
    }
  }
  //that's how tight it is.
  REQUIRE(max_lean == (kernel_.max_index_ - 1));

  free(game_);
}
