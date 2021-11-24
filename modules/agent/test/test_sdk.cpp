//
// Created by Isaac Zhang on 7/30/20.
//
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../src/bulldog_sdk.h"
#include "../src/bulldog_controller.hpp"

void cLogger(char *format, ...){
  logger::debug(format);
}

TEST_CASE("test bulldog sdk", "[sdk]") {
  fs::path dir(BULLDOG_DIR_CFG_GAME);
  fs::path filename("holdem.nolimit.2p.game");
  Game *game = nullptr;
  FILE *file = fopen((dir / filename).c_str(), "r");
  game = readGame(file);

  logger::init_logger("debug");

  BulldogController server;
  server.setEndpoint(SERVER_DEFAULT_ENDPOINT);
  REQUIRE(server.LoadDefault("engine_random_action.json", GAME_NLH2));

//  auto cLogger = [](char *format, ...) -> void {};

  try {
    // wait for server initialization...
    server.accept().wait();
    logger::info("listing for requests at:%s ", server.endpoint());

    BulldogSDK sdk;
    BulldogSDK sdk2;
    BulldogSDK sdk3;
    BulldogSDK sdk4;
    /*
     * hack to the right ip
     */
    REQUIRE(sdk.AutoSearchEndpoint(cLogger));
    REQUIRE(sdk.CreateTableSession(cLogger, GAME_NLH2, TABLE_POKERSTAR_LITE_NLH2_50_100));
    REQUIRE(sdk.SendHeartbeat(cLogger));

    REQUIRE(sdk2.AutoSearchEndpoint(cLogger));
    REQUIRE(sdk2.CreateTableSession(cLogger, GAME_NLH2, TABLE_POKERSTAR_LITE_NLH2_50_100));

    REQUIRE(sdk3.AutoSearchEndpoint(cLogger));
    REQUIRE(sdk3.CreateTableSession(cLogger, GAME_NLH2, TABLE_POKERSTAR_LITE_NLH2_50_100));

    REQUIRE(sdk4.AutoSearchEndpoint(cLogger));
    REQUIRE(!sdk4.CreateTableSession(cLogger, GAME_NLH2, TABLE_POKERSTAR_LITE_NLH2_50_100));
    /*
     * edit session, e.g. change blinds
     */
    int sb = 7;
    int bb = 14;
    REQUIRE(sdk.ChangeBlinds(cLogger, sb, bb));
    game->blind[0] = sb;
    game->blind[1] = bb;

    /*
     * new hand
     */
    REQUIRE(sdk.NewHand(cLogger));
    REQUIRE(!sdk.NewHand(cLogger)); //you need to first end it

    /*
     * get action
     */
    std::string game_feed = "MATCHSTATE:1:1:4960|5080:r50r180c/r240:|4sQs/8s9s7s";
    MatchState match_state;
    REQUIRE(readMatchState(game_feed.c_str(), game, &match_state) > 0);
    Action action;
    REQUIRE(sdk.GetAction(cLogger, game, match_state, action, 10000));
//    logger::info("return action code %d", actionToCode(&action));
    REQUIRE(isValidAction(game, &match_state.state, 0, &action));

    /*
     * end hand
     */
    REQUIRE(sdk.EndHand(cLogger));

    sleep(12);
    REQUIRE(sdk4.AutoSearchEndpoint(cLogger));
    REQUIRE(sdk4.CreateTableSession(cLogger, GAME_NLH2, TABLE_POKERSTAR_LITE_NLH2_50_100));

    /*
     * delete session
     */
    //REQUIRE(sdk.DeleteTableSession());

    server.Cleanup();
    server.shutdown().wait();
    logger::info("server shutting down");
  }
  catch (std::exception &e) {
    logger::error("%s", e.what());
  }
}