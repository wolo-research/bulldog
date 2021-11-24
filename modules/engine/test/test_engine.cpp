//
// Created by Carmen C on 15/5/2020.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <bulldog/engine.h>
#include <pokerstove/peval/CardSet.h>
#include <pokerstove/peval/Card.h>

extern "C" {
#include <bulldog/game.h>
};

TEST_CASE("test strategy pool") {
  StrategyPool pool;
  //30 depth
  AbstractGame *ag1 = new AbstractGame;
  ag1->game_.blind[0] = 50;
  ag1->game_.blind[1] = 100;
  ag1->game_.stack[0] = 3000;
  ag1->game_.stack[1] = 3000;
  Strategy *s1 = new Strategy(ag1);
  REQUIRE(GameDefaultStackDepth(&ag1->game_) == 30);

  //50 depth
  AbstractGame *ag2 = new AbstractGame;
  ag2->game_.blind[0] = 50;
  ag2->game_.blind[1] = 100;
  ag2->game_.stack[0] = 5000;
  ag2->game_.stack[1] = 5000;
  Strategy *s2 = new Strategy(ag2);
  REQUIRE(GameDefaultStackDepth(&ag2->game_) == 50);

  //70 depth
  AbstractGame *ag3 = new AbstractGame;
  ag3->game_.blind[0] = 50;
  ag3->game_.blind[1] = 100;
  ag3->game_.stack[0] = 7000;
  ag3->game_.stack[1] = 7000;
  Strategy *s3 = new Strategy(ag3);
  REQUIRE(GameDefaultStackDepth(&ag3->game_) == 70);

  pool.AddStrategy(s1);
  pool.AddStrategy(s2);
  pool.AddStrategy(s3);

  //45 depth match
  MatchState match1;
  match1.state.stackPlayer[0] = 4500;
  match1.state.stackPlayer[1] = 9700;
  //does not matter which game, as we are using the stack info on the state
  REQUIRE(pool.FindStrategy(&match1, &ag1->game_) == s2);

  //55 depth match
  MatchState match2;
  match2.state.stackPlayer[0] = 14500;
  match2.state.stackPlayer[1] = 5500;
  REQUIRE(pool.FindStrategy(&match2, &ag1->game_) == s3);

  //75 depth match
  MatchState match3;
  match3.state.stackPlayer[0] = 14500;
  match3.state.stackPlayer[1] = 7500;
  REQUIRE(pool.FindStrategy(&match3, &ag1->game_) == s3);
}

TEST_CASE("test server-engine interface") {
  logger::init_logger("debug");
  //Initiate Game
  Game *game = nullptr;
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path filename("holdem.nolimit.2p.game");
  FILE *file = fopen((dir / filename).c_str(), "r");
  if (file == nullptr) {
    logger::critical("Failed to find file %s", (dir / filename));
  }
  game = readGame(file);
  if (game == nullptr) {
    logger::critical("Failed to read content of game file %s", (dir / filename));
  }

  auto *engine = new Engine("engine_random_action.json", game);

  /*
   * set to stack aware
   */
  engine->GetGame()->use_state_stack = 1;

  /*
   * test session creation etc
   */
  Game *session_game = nullptr;
  FILE *session_game_file = fopen((dir / filename).c_str(), "r");
  session_game = readGame(session_game_file);
  if (session_game == nullptr) {
    logger::critical("Failed to read content of game file %s", (dir / filename));
  }
  TableContext session;
  session.session_game = *session_game;
  session.session_game.blind[0] = 25;
  session.session_game.blind[1] = 50;
  REQUIRE(BigBlind(&session.session_game) == 50);
  REQUIRE(engine->SetTableContext(session) == NEW_SESSION_SUCCESS);
  REQUIRE(engine->SetTableContext(session) == NEW_SESSION_SUCCESS);
  REQUIRE(engine->RefreshEngineState() == NEW_HAND_SUCCESS);

//    std::string state_str1 = "MATCHSTATE:1:1:10000|10000:cc/:|2hTs/Tc9s9d";
  std::string state_str1 = "MATCHSTATE:1:5:10000|10000:r200c/r400c/cr800:|KcQs/AdQh5h/9c";

  /*
   * state translation
   */
  printf("test invalid state string\n");
  MatchState norm_state;
  REQUIRE(engine->TranslateToNormState("haha" + state_str1, norm_state) == MATCH_STATE_PARSING_FAILURE);

  printf("test valid state string\n");
  MatchState norm_state_1;
  REQUIRE(engine->TranslateToNormState(state_str1, norm_state_1) == MATCH_STATE_PARSING_SUCCESS);
  char norm_state_str[MAX_LINE_LEN];
  printMatchState(engine->GetGame(), &norm_state_1, MAX_LINE_LEN, norm_state_str);
  REQUIRE(strcmp(norm_state_str, "MATCHSTATE:1:5:20000|20000:r400c/r800c/cr1600:|KcQs/AdQh5h/9c") == 0);

  Action action;
  engine->GetActionBySession(norm_state_1, action, 0);

  delete engine;
  free(game);
}

TEST_CASE("engine mapping") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game = readGame(file);

  std::string statestr = "MATCHSTATE:0:0:20000|20000:r212r600r1450c/cr4350c/r7200r18840r20000:Qc6h|/JhJdTc/8c";
  MatchState state;
  readMatchState(statestr.c_str(), game, &state);

//  auto engine = new Engine("engine_slumbot_dummy_sgs.json", game);
//  Action action = engine->GetAction(&state, 10);
  Action action;
  action.type = a_raise;
  action.size = 20000;
  REQUIRE(!isValidAction(game, &state.state, 0, &action));
}

TEST_CASE("test action chooser") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path game_file("holdem.nolimit.2p.game");
  FILE *file = fopen((dir / game_file).c_str(), "r");
  if (file == nullptr) {
    logger::critical("failed to open game file %s", game_file);
  }
  Game *game = readGame(file);

  MatchState state;
  initState(game, 0, &state.state);
  Node node{};
  node.SetState(state.state);

  const char *child2_str = "MATCHSTATE:0:0::c:2ckd|2s3d";
  MatchState child2_state;
  readMatchState(child2_str, game, &child2_state);
  Node child2{};
  child2.SetState(child2_state.state);
  node.children.push_back(&child2);

  const char *child3_str = "MATCHSTATE:0:0::r200:2ckd|2s3d";
  MatchState child3_state;
  readMatchState(child3_str, game, &child3_state);
  Node child3{};
  child3.SetState(child3_state.state);
  node.children.push_back(&child3);

  node.SortChildNodes();

  ActionChooser ac;
  ac.params_.fold_threshold_ = 0.2;
  ac.params_.purification_ = true;
  {
    float avg[2] = {0.2, 0.1};
    Action a;
    ac.ChooseAction(avg, 2, &node, a);
    REQUIRE(a.type == a_call);
  }
  {
    float avg[2] = {0.1, 0.2};
    Action a;
    ac.ChooseAction(avg, 2, &node, a);
    REQUIRE(a.type == a_raise);
    REQUIRE(a.size == 200);
  }
  {
    float avg[2] = {0.0, 0.0};
    Action a;
    ac.ChooseAction(avg, 2, &node, a);
    REQUIRE(a.type == a_fold);
  }

  const char *child1_str = "MATCHSTATE:0:0::f:2ckd|2s3d";
  MatchState child1_state;
  readMatchState(child1_str, game, &child1_state);
  Node child1{};
  child1.SetState(child1_state.state);
  node.children.push_back(&child1);
  node.SortChildNodes();
  {
    float avg[3] = {0.2, 0.3, 0.3};
    // > fold thres
    Action a;
    ac.ChooseAction(avg, 3, &node, a);
    REQUIRE(a.type == a_fold);
  }
  {
    float avg[3] = {0.19, 0.3, 0.29};
    // purify to call
    Action a;
    ac.ChooseAction(avg, 3, &node, a);
    REQUIRE(a.type == a_call);
  }
  {
    float avg[3] = {0.19, 0.03, 0.12};
    // purify to fold
    Action a;
    ac.ChooseAction(avg, 3, &node, a);
    REQUIRE(a.type == a_fold);
  }
}