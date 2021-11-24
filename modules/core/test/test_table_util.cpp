//
// Created by Isaac Zhang on 8/18/20.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <filesystem>
#include "bulldog/table_util.h"
#include "bulldog/game_util.h"

extern "C" {
#include "bulldog/game.h"
};

TEST_CASE("table action test", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game2 = nullptr;
  FILE *file2 = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game2 = readGame(file2);
  game2->use_state_stack = 1;
  SECTION("action translation: WITH prior action") {
    std::string a1 = "STATE:0:4500|4600:cc/cc/cc/cr300r600:Tc2d|2s3d/7s8s9s/5c/6c";
    State state_a1;
    REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
    {
      Action action;
      action.type = a_fold;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_FOLD);
    }
    {
      Action action;
      action.type = a_call;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_CALL);
    }
    {
      Action action;
      action.type = a_raise;
      action.size = 4500;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_ALLIN_RAISE);
    }
    //base is 500
    {
      Action action;
      action.type = a_raise;
      action.size = 1100;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_2X_RAISE);
    }
    {
      Action action;
      action.type = a_raise;
      action.size = 1600;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_3X_RAISE);
    }
    {
      Action action;
      action.type = a_raise;
      action.size = 2100;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_4X_RAISE);
    }
  }

  SECTION("action translation: without prior action") {
    std::string a1 = "STATE:0:4500|4600:cc/cc/cr800c/:Tc2d|2s3d/7s8s9s/5c/6c";
    State state_a1;
    REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
    {
      Action action;
      action.type = a_raise;
      action.size = 1600;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_HALF_POT_RAISE);
    }
    {
      Action action;
      action.type = a_raise;
      action.size = 1867;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_TWO_THIRD_POT_RAISE);
    }
    {
      Action action;
      action.type = a_raise;
      action.size = 2400;
      auto code = ActionToTableAction(game2, state_a1, action);
      REQUIRE(code == TABLE_ACTION_ONE_POT_RAISE);
    }
  }

  SECTION("table action check") {
    {
      /*
       * always true actions
       */
      std::string a1 = "STATE:0:4500|4600:cc/cc/cr800c/:Tc2d|2s3d/7s8s9s/5c/6c";
      State state_a1;
      REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
      REQUIRE(IsTableActionPresent(game2, state_a1, TABLE_ACTION_FOLD));
      REQUIRE(IsTableActionPresent(game2, state_a1, TABLE_ACTION_BETSIZE));
      REQUIRE(IsTableActionPresent(game2, state_a1, TABLE_ACTION_CHECK));
      REQUIRE(!IsTableActionPresent(game2, state_a1, TABLE_ACTION_CALL));
    }

    {
      /*
       * check all-in
       */
      std::string a1 = "STATE:0:4500|4600:cc/cc/cr800c/:Tc2d|2s3d/7s8s9s/5c/6c";
      State state_a1;
      REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
      REQUIRE(IsTableActionPresent(game2, state_a1, TABLE_ACTION_ALLIN_RAISE));
      Action allin;
      allin.type = a_raise;
      allin.size = 4600;
      doAction(game2, &allin, &state_a1);
      //now u can not shove allin, can only call
      REQUIRE(IsTableActionPresent(game2, state_a1, TABLE_ACTION_ALLIN_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a1, TABLE_ACTION_CALL));
      REQUIRE(!IsTableActionPresent(game2, state_a1, TABLE_ACTION_CHECK));
    }
    {
      /*
       * check raise actions, for open pot 0.5/0.667/1.0 -> 1600/1867/2400
       */
      std::string a1 = "STATE:0:4500|1700:cc/cc/cr800c/:Tc2d|2s3d/7s8s9s/5c/6c";
      State state_a1;
      REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
      REQUIRE(IsTableActionPresent(game2, state_a1, TABLE_ACTION_HALF_POT_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a1, TABLE_ACTION_TWO_THIRD_POT_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a1, TABLE_ACTION_ONE_POT_RAISE));

      std::string a2 = "STATE:0:4500|2700:cc/cc/cr800c/:Tc2d|2s3d/7s8s9s/5c/6c";
      State state_a2;
      REQUIRE(readState(a2.c_str(), game2, &state_a2) > 0);
      REQUIRE(IsTableActionPresent(game2, state_a2, TABLE_ACTION_HALF_POT_RAISE));
      REQUIRE(IsTableActionPresent(game2, state_a2, TABLE_ACTION_TWO_THIRD_POT_RAISE));
      REQUIRE(IsTableActionPresent(game2, state_a2, TABLE_ACTION_ONE_POT_RAISE));

      //and other raise should be invalid
      REQUIRE(!IsTableActionPresent(game2, state_a2, TABLE_ACTION_2X_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a2, TABLE_ACTION_3X_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a2, TABLE_ACTION_4X_RAISE));
    }
    {
      /*
       * check raise actions, with prior raise actions 2x/3x/4x -> 1100/1600/2100
       */
      std::string a1 = "STATE:0:1500|1500:cc/cc/cc/cr300r600:Tc2d|2s3d/7s8s9s/5c/6c";
      State state_a1;
      REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
      REQUIRE(IsTableActionPresent(game2, state_a1, TABLE_ACTION_2X_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a1, TABLE_ACTION_3X_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a1, TABLE_ACTION_4X_RAISE));

      std::string a2 = "STATE:0:4500|2500:cc/cc/cc/cr300r600:Tc2d|2s3d/7s8s9s/5c/6c";
      State state_a2;
      REQUIRE(readState(a2.c_str(), game2, &state_a2) > 0);
      REQUIRE(IsTableActionPresent(game2, state_a2, TABLE_ACTION_2X_RAISE));
      REQUIRE(IsTableActionPresent(game2, state_a2, TABLE_ACTION_3X_RAISE));
      REQUIRE(IsTableActionPresent(game2, state_a2, TABLE_ACTION_4X_RAISE));

      //and other raise should be invalid
      REQUIRE(!IsTableActionPresent(game2, state_a2, TABLE_ACTION_HALF_POT_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a2, TABLE_ACTION_TWO_THIRD_POT_RAISE));
      REQUIRE(!IsTableActionPresent(game2, state_a2, TABLE_ACTION_ONE_POT_RAISE));
    }
  }

  SECTION("map to action") {
    {
      std::string a1 = "STATE:0:6500|6600:cc/cc/cr800c/r1800:Tc2d|2s3d/7s8s9s/5c/6c";
      State state_a1;
      REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
      //2x = 2800 | 3x 3800 4x 4800
      Action raise;
      raise.type = a_raise;
      raise.size = 3200;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_2X_RAISE);
      raise.size = 3600;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_3X_RAISE);
      raise.size = 4200;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_3X_RAISE);
      raise.size = 4700;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_4X_RAISE);
      raise.size = 5700;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_4X_RAISE);
      //if tie, favor the one with with less
      raise.size = 4300;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_3X_RAISE);
      //sometimes it may be closest to call
      raise.size = 2200;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_CALL);
    }
    {
      std::string a1 = "STATE:0:6500|6600:cc/cc/cr1200c/:Tc2d|2s3d/7s8s9s/5c/6c";
      State state_a1;
      REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
      //0.5x = 2400 | 2/3x = 2800 | 1x = 3600
      Action raise;
      raise.type = a_raise;
      raise.size = 2200;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_HALF_POT_RAISE);
      raise.size = 2650;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_TWO_THIRD_POT_RAISE);
      raise.size = 3000;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_TWO_THIRD_POT_RAISE);
      raise.size = 3700;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_ONE_POT_RAISE);
      raise.size = 5700;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_ONE_POT_RAISE);
      //if tie, favor the one with with less
      raise.size = 2600;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_HALF_POT_RAISE);
      //sometimes it may be closest to call
      raise.size = 1700;
      REQUIRE(MapToTableAction(game2, state_a1,raise) == TABLE_ACTION_CALL);
    }
  }
}

TEST_CASE("test delay action", "[core]") {
  int milli_available = 8500;
  int max_delay = 5000;

  auto start_time = std::chrono::system_clock::now();
  usleep(5000 * 1000); // takes microsecond
  int delay = DelayAction(start_time, milli_available, max_delay);
  REQUIRE(0 == delay);

  start_time = std::chrono::system_clock::now();
  usleep(3000 * 1000); // takes microsecond
  delay = DelayAction(start_time, milli_available, max_delay);
  REQUIRE(delay >= 0);
  REQUIRE(delay < max_delay);
}