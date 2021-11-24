//
// Created by Carmen C on 11/3/2020.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

extern "C" {
#include "bulldog/game.h"
};

#include <filesystem>
namespace fs = std::filesystem;
#include "../src/slumbot_connector.hpp"

//slumbot matchstates in acpc format
std::string MATCHSTATES[4] = {
    "MATCHSTATE:0:0::b300b900b19750b20000:Kh6s|",
    "MATCHSTATE:0:0::b200c/kb200:Ts4h|/QcJh5s",
    "MATCHSTATE:1:0::b200b600b1800c/kb1800b9000b18200:|Kh2d/Kc5s2h",
    "MATCHSTATE:1:0::b200b600c/b1200c/kb3600b15600b18200:|As5s/9h7h7d2d",
};

//todo: move it away
TEST_CASE("test readMatchPlus", "[connector]"){
  fs::path dir(BULLDOG_DIR_CFG_GAME);
  fs::path filename("holdem.nolimit.2p.game");
  Game *game = nullptr;
  FILE *file = fopen((dir/filename).c_str(), "r");
  game = readGame(file);

  SECTION("test slumbot parse"){
    MatchState match_state;
    for(auto& state : MATCHSTATES){
        unsigned long len = readMatchStatePlus(state.c_str(), game, &match_state, bsbrReadBetting);
        REQUIRE(len==std::strlen(state.c_str()));
    }
  }
  free(game);
}