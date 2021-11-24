//
// Created by Carmen C on 17/2/2020.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <filesystem>
#include <bulldog/core_util.hpp>
#include <bulldog/logger.hpp>
#include <bulldog/game_util.h>
#include "../../agent/src/base_connector.hpp"
extern "C" {
#include "bulldog/game.h"
};

SCENARIO("test match state card crash", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game2 = nullptr;
  FILE *file2 = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game2 = readGame(file2);

  MatchState state;
  initState(game2, 0, &state.state);
  state.viewingPlayer = 0;
  state.state.holeCards[0][0] = 2;
  state.state.boardCards[0] = 3;
  state.state.boardCards[1] = 4;
  state.state.boardCards[2] = 6;

  WHEN("wrong hole card"){
    state.state.holeCards[0][1] = 4;
    state.state.round = 1;
    REQUIRE(!IsStateCardsValid(game2, &state.state));
  }
  WHEN("right hole card"){
    state.state.holeCards[0][1] = 9;
    state.state.round = 1;
    REQUIRE(IsStateCardsValid(game2, &state.state));
  }
  WHEN("wrong board card"){
    state.state.holeCards[0][1] = 9;
    state.state.round = 2;
    state.state.boardCards[3] = 6;
    REQUIRE(!IsStateCardsValid(game2, &state.state));
  }

  free(game2);
}

TEST_CASE("test IsPriorState check", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game2 = nullptr;
  FILE *file2 = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game2 = readGame(file2);
  //the alright batch
  std::string a1 = "STATE:0::cc/:9c2d|2s3d/7s8s9s",
      a2 = "STATE:0::cc/c:9c2d|2s3d/7s8s9s",
      a3 = "STATE:0::cc/cc:2c2d|2s3d/7s8s9s/3d",
      a4 = "STATE:0::cc/cr300:9c2d|2s3d/7s8s9s",
      a5 = "STATE:0::cc/cc/cc/cr300:Tc2d|2s3d/7s8s9s/5c/6c";

  State state_a1;
  REQUIRE(readState(a1.c_str(), game2, &state_a1) > 0);
  State state_a2;
  REQUIRE(readState(a2.c_str(), game2, &state_a2) > 0);
  REQUIRE(IsPriorStateSameRound(&state_a1, &state_a2));
  REQUIRE(!IsPriorStateSameRound(&state_a1, &state_a1));
  REQUIRE(!IsPriorStateSameRound(&state_a2, &state_a1));

  State state_a4;
  REQUIRE(readState(a4.c_str(), game2, &state_a4) > 0);
  REQUIRE(IsPriorStateSameRound(&state_a1, &state_a4));
  REQUIRE(IsPriorStateSameRound(&state_a2, &state_a4));
  REQUIRE(!IsPriorStateSameRound(&state_a4, &state_a2));
}

TEST_CASE("matchstate of bet sum by round", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.3p.game").c_str(), "r");
  game = readGame(file);
  std::string game_feed = "MATCHSTATE:0:2::ccc/r500fc/r4540c/:KcAs|/Tc6sQc/4s/4h";
//  std::string game_feed = "STATE:54:r300c/cr750r20000c//:7dKd|QhTd/Qd3d7s/6s/4s";
  MatchState match_state;
  match_state.state.stackPlayer[0] = 5060;
  match_state.state.stackPlayer[1] = 4960;
  match_state.state.stackPlayer[2] = 4980;
  game->use_state_stack = 1;
  //this is bet sum by round, so it is finished.
  REQUIRE(readMatchStatePlus(game_feed.c_str(), game, &match_state, bsbrReadBetting) > 0);
  REQUIRE(stateFinished(&match_state.state) == 1);
  free(game);
}

TEST_CASE("test acpc protocol corner cases", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game = readGame(file);
  {
    std::string game_feed = "MATCHSTATE:1:1:::|5dJd";
    MatchState match_state;
    REQUIRE(readMatchState(game_feed.c_str(), game, &match_state) > 0);
  }
  {
    std::string game_feed = "MATCHSTATE:0:0:::5d5c|";
    MatchState match_state;
    REQUIRE(readMatchState(game_feed.c_str(), game, &match_state) > 0);
  }
  free(game);
}

TEST_CASE("matchstate acpc stack-awared", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.3p.game").c_str(), "r");
  game = readGame(file);
  /*
   * without the stack in the string
   */
  {
    std::string game_feed = "MATCHSTATE:0:2::ccc/r500fc/r4540c/:KcAs|/Tc6sQc/4s/4h";
//  std::string game_feed = "STATE:54:r300c/cr750r20000c//:7dKd|QhTd/Qd3d7s/6s/4s";
    MatchState match_state;
    match_state.state.stackPlayer[0] = 5060;
    match_state.state.stackPlayer[1] = 4960;
    match_state.state.stackPlayer[2] = 4980;
    game->use_state_stack = 1;
    //this is bet sum by round, so it finishes.
    REQUIRE(readMatchState(game_feed.c_str(), game, &match_state) > 0);
    REQUIRE(stateFinished(&match_state.state) == 0);
  }
  /*
   * with the stack
   */
  {
    std::string game_feed = "MATCHSTATE:0:2:4960|4960|5080:ccc/r500fc/r4540c/:KcAs|/Tc6sQc/4s/4h";
//  std::string game_feed = "STATE:54:r300c/cr750r20000c//:7dKd|QhTd/Qd3d7s/6s/4s";
    MatchState match_state;
    game->use_state_stack = 1;
    //this is bet sum by round, so it finishes.
    REQUIRE(readMatchState(game_feed.c_str(), game, &match_state) > 0);
    REQUIRE(match_state.state.stackPlayer[0] == 4960);
    REQUIRE(match_state.state.stackPlayer[1] == 4960);
    REQUIRE(match_state.state.stackPlayer[2] == 5080);
    REQUIRE(stateFinished(&match_state.state) == 0);
  }
  free(game);
}

TEST_CASE("stack-aware action raise detection", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.3p.game").c_str(), "r");
  game = readGame(file);
  std::string game_feed = "MATCHSTATE:2:2:4960|5080|4960:cc:||4sQs";
  MatchState match_state;
  REQUIRE(readMatchState(game_feed.c_str(), game, &match_state) > 0);

  Action action;
  action.size = 4960;
  action.type = a_raise;

  REQUIRE(isValidAction(game, &match_state.state, 0, &action) == 1);
  action.size += 210;
  REQUIRE(isValidAction(game, &match_state.state, 0, &action) == 0);
  //try fix and test if it is fixed back
  REQUIRE(isValidAction(game, &match_state.state, 1, &action) == 1);
  REQUIRE(action.size == 5080);

  /*
   * detect oversize raise against all-in players
   */
  doAction(game, &action, &match_state.state);
  //should "MATCHSTATE:2:2:4960|5080|4960:ccr4960:||4sQs";
  Action over_raise;
  over_raise.type = a_raise;
  over_raise.size = 5080;
  REQUIRE(!isValidAction(game, &match_state.state, 0, &over_raise));

  REQUIRE(4960 == match_state.state.stackPlayer[0]);
  REQUIRE(5080 == match_state.state.stackPlayer[1]);
  REQUIRE(4960 == match_state.state.stackPlayer[2]);

  free(game);
}

TEST_CASE("read stack") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.3p.game").c_str(), "r");
  game = readGame(file);
  State state;

  const char stack[] = "4960|4960|5080";
  int res = readStack(stack, game, &state);
  REQUIRE(4960 == state.stackPlayer[0]);
  REQUIRE(4960 == state.stackPlayer[1]);
  REQUIRE(5080 == state.stackPlayer[2]);
  REQUIRE(14 == res);

  free(game);
}

TEST_CASE("test poker game", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.limit.2p.reverse_blinds.game").c_str(), "r");
  game = readGame(file);

  SECTION("assert game file content") {
    REQUIRE(game->bettingType == 0);
    REQUIRE(game->numPlayers == 2);
    REQUIRE(game->numSuits == 4);
    REQUIRE(game->numRanks == 13);
    REQUIRE(game->numHoleCards == 2);
  }
  printf("assert game file done");

  MatchState match_state;
  std::string game_feed;
  game_feed = "MATCHSTATE:0:0::rrc/rc/crc/crc:TdAs|8hTc/2c8c3h/9c/Kh";
  REQUIRE(readMatchState(game_feed.c_str(), game, &match_state) > 0);

  SECTION("read game match state") {
    REQUIRE(match_state.state.finished == 1);
    REQUIRE(match_state.viewingPlayer == 0);
  }
  printf("read game match done");

  SECTION("game state reading") {
    Game *game2 = nullptr;
    FILE *file2 = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
    game2 = readGame(file2);
    //the alright batch
    std::string a1 = "STATE:0::cc/:9c2d|2s3d/7s8s9s",
        a2 = "STATE:0::cc/c:9c2d|2s3d/7s8s9s",
        a3 = "STATE:0::cc/cc:2c2d|2s3d/7s8s9s/3d",
        a4 = "STATE:0::cc/cr300:9c2d|2s3d/7s8s9s",
        a5 = "STATE:0::cc/cc/cc/cr300:Tc2d|2s3d/7s8s9s/5c/6c";
    {
      State state;
      REQUIRE(readState(a1.c_str(), game2, &state) > 0);
    }
    {
      State state;
      REQUIRE(readState(a2.c_str(), game2, &state) > 0);
    }
    {
      State state;
      REQUIRE(readState(a3.c_str(), game2, &state) > 0);
    }
    {
      State state;
      REQUIRE(readState(a4.c_str(), game2, &state) > 0);
    }
    {
      State state;
      REQUIRE(readState(a5.c_str(), game2, &state) > 0);
    }


    //the finished game batch
    std::string f1 = "STATE:0::f:9c2d|2s3d",
        f2 = "STATE:0::cc/cc/cc/cr300f:Tc2d|2s3d/7s8s9s/5c/6c",
        f3 = "STATE:0::cc/cc/cc/cr300c:Tc2d|2s3d/7s8s9s/5c/6c";
    {
      State state;
      REQUIRE(readState(f1.c_str(), game2, &state) > 0);
      REQUIRE(state.finished);
    }
    {
      State state;
      REQUIRE(readState(f2.c_str(), game2, &state) > 0);
      REQUIRE(state.finished);
    }
    {
      State state;
      REQUIRE(readState(f3.c_str(), game2, &state) > 0);
      REQUIRE(state.finished);
    }

    //the dead batch.
    std::string d2 = "STATE:0::cc/cr300:10c2d|2s3d/7s8s9s";
    std::string d3 = "STATE:0::cc/cc:2c2d|2s3d/7s8s9s";
    {
      State state;
      REQUIRE(readState(d2.c_str(), game2, &state) < 0);
    }
    {
      State state;
      REQUIRE(readState(d3.c_str(), game2, &state) < 0);
    }

    free(game2);
  }

  SECTION("print states") {
//    int rank0 = rankHand(game, &match_state.state, 0);
//    int rank1 = rankHand(game, &match_state.state, 1);
//    double value0 = valueOfState(game, &match_state.state, 0);
//    double value1 = valueOfState(game, &match_state.state, 1);

    //int c, d;
    char line[MAX_LINE_LEN], line2[MAX_LINE_LEN];
    printState(game, &match_state.state, MAX_LINE_LEN, line);
    //limit holdem has stack as default INT32_MaX
    REQUIRE(strcmp(line, "STATE:0:2147483647|2147483647:rrc/rc/crc/crc:TdAs|8hTc/2c8c3h/9c/Kh") == 0);
    printMatchState(game, &match_state, MAX_LINE_LEN, line2);
    REQUIRE(strcmp(line2, "MATCHSTATE:0:0:2147483647|2147483647:rrc/rc/crc/crc:TdAs|8hTc/2c8c3h/9c/Kh") == 0);
  }

  free(game);
}

TEST_CASE("split string") {
  std::string test_str = "wife>=husband>=pet";
  std::vector<std::string> parsed_str;
  split_string(test_str, ">=", parsed_str);
  REQUIRE("wife" == parsed_str[0]);
  REQUIRE("husband" == parsed_str[1]);
  REQUIRE("pet" == parsed_str[2]);
}

TEST_CASE("subgame solving related", "[core]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game2 = nullptr;
  FILE *file2 = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game2 = readGame(file2);
  std::string d2 = "MATCHSTATE:0:0::cr300c/cr600r1800c/cr5000c/:2c2d|2s3d/7sKh9s/Qd/6d";
  SECTION("last_action") {
    MatchState state;
    REQUIRE(readMatchState(d2.c_str(), game2, &state) > 0);
    Action last_action = GetLastActionFromState(&state.state);
    REQUIRE(last_action.type == a_call);
  }

  SECTION("last_action with early all-in") {
    std::string d3 = "MATCHSTATE:0:0::cr300r20000c///:2c2d|2s3d/7sKh9s/Qd/6d";
    MatchState state;
    REQUIRE(readMatchState(d3.c_str(), game2, &state) > 0);
    Action last_action = GetLastActionFromState(&state.state);
    REQUIRE(last_action.type == a_call);
  }

  SECTION("step back states") {
    MatchState state;
    REQUIRE(readMatchState(d2.c_str(), game2, &state) > 0);
    State ref_state = state.state;

    auto total_action = TotalAction(&ref_state);
    REQUIRE(total_action == 10);
    //out of bount
    {
      State step_back_state;
      REQUIRE(StepBackAction(game2, &ref_state, &step_back_state, 11) == -1);
    }
    {
      State step_back_state;
      REQUIRE(StepBackAction(game2, &ref_state, &step_back_state, 10) == 0);
    }
    {
      State step_back_state;
      REQUIRE(StepBackAction(game2, &ref_state, &step_back_state, 0) == 0);
      //state all equal if back step = 0
      REQUIRE(StateEqual(game2, &ref_state, &step_back_state));
    }
    //action index
    int action_index = 1;
    for (int r = 0; r <= ref_state.round; r++) {
      for (int a = 0; a < ref_state.numActions[r]; a++) {
        Action real_action = ref_state.action[r][a];
        State step_back_state;
        logger::info("testing taking %d steps back", total_action - action_index);
        REQUIRE(StepBackAction(game2, &ref_state, &step_back_state, total_action - action_index) == 0);
        Action last_action = GetLastActionFromState(&step_back_state);
        REQUIRE(last_action.type == real_action.type);
        if (last_action.type == a_raise)
          REQUIRE(last_action.size == real_action.size);
        action_index++;
      }
    }
  }

  free(game2);
}

TEST_CASE("valid raise ") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game = readGame(file);
  std::string d = "MATCHSTATE:0:0::cc/cc/cr20000:2c2d|2s3d/7sKh9s/Qd/6d";\

  MatchState state;
  readMatchState(d.c_str(), game, &state);

  int min, max;
  int flag = raiseIsValid(game, &state.state, &min, &max);
  REQUIRE(flag == 0);
}

TEST_CASE("pot raise mode") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game = readGame(file);
  {
    std::string d = "MATCHSTATE:0:0::cc/cc/cr300:2c2d|2s3d/7sKh9s/Qd/6d";
    MatchState state;
    readMatchState(d.c_str(), game, &state);
    REQUIRE(GetRaiseBase(game, &state.state, POT_AFTER_CALL) == 600);
    REQUIRE(GetRaiseBase(game, &state.state, POT_NET) == 400);
    REQUIRE(GetRaiseBase(game, &state.state, X_LAST_RAISE) == 200);
  }
  {
    std::string d = "MATCHSTATE:0:0::c:2c2d|";
    MatchState state;
    readMatchState(d.c_str(), game, &state);
    REQUIRE(GetRaiseBase(game, &state.state, POT_AFTER_CALL) == 200);
    REQUIRE(GetRaiseBase(game, &state.state, POT_NET) == 200);
    REQUIRE(GetRaiseBase(game, &state.state, X_LAST_RAISE) == 100);
  }
  {
    //the same for unopen game
    std::string d = "MATCHSTATE:0:0:::2c2d|";
    MatchState state;
    readMatchState(d.c_str(), game, &state);
    REQUIRE(GetRaiseBase(game, &state.state, POT_AFTER_CALL) == 200);
    REQUIRE(GetRaiseBase(game, &state.state, POT_NET) == 150);
    REQUIRE(GetRaiseBase(game, &state.state, X_LAST_RAISE) == 100);
  }
}

TEST_CASE("slumbot state debug") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game = readGame(file);

  std::string acpc_format = "MATCHSTATE:0:0::b212b600b1450c/kb2900c/b2850b14490b15650:Qc6h|/JhJdTc/8c";
  MatchState state;
  int len = readMatchStatePlus(acpc_format.c_str(), game, &state, bsbrReadBetting);
  REQUIRE(len != 0);

  char line[1024];
  printMatchState(game, &state, 1024, line);
  printf("%s\n", line);

  int min, max;
  int flag = raiseIsValid(game, &state.state, &min, &max);
  REQUIRE(flag == 0);
}
