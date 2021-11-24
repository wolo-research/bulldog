//
// Created by Isaac Zhang on 4/11/20.
// for head-2-head eval
//

#include <bulldog/engine.h>
#include <bulldog/logger.hpp>
extern "C" {
#include "bulldog/game.h"
}
#include <cstdlib>
#include <cxxopts.hpp>
#include <filesystem>
#include "bulldog_controller.hpp"
#include "bulldog_sdk.h"

uint8_t seatToPlayer(const Game *game, const uint8_t player0Seat,
                            const uint8_t seat) {
  return (seat + game->numPlayers - player0Seat) % game->numPlayers;
}

static uint8_t playerToSeat(const Game *game, const uint8_t player0Seat,
                            const uint8_t player) {
  return (player + player0Seat) % game->numPlayers;
}

void cLogger(char *format, ...){
  logger::debug(format);
}

int main(int argc, char *argv[]) {
  cxxopts::Options options("Evaluate - Head 2 Head", "Provide h2h evaluation statistics");
  options.add_options()
      ("m,match", "match name", cxxopts::value<std::string>())
      ("g,game", "game file", cxxopts::value<std::string>())
      ("a,hands", "#hands to play", cxxopts::value<int>())
      ("s,seed", "seed for rng", cxxopts::value<int>())
      ("u,subgame", "file to read subgame", cxxopts::value<std::string>())
      ("n,players", "name of players", cxxopts::value<std::vector<std::string>>())
      ("h,help", "print usage information");
  auto result = options.parse(argc, argv);
  if (result.count("help") || (result.arguments()).empty()) {
    std::cout << options.help() << std::endl;
    exit(0);
  }

  bool check_must_opts = result.count("match")
      && result.count("game")
      && result.count("hands")
      && result.count("seed")
      && result.count("players");
  if (!check_must_opts) {
    std::cout << "fill the must-have options" << std::endl;
    std::cout << options.help() << std::endl;
    exit(EXIT_FAILURE);
  }

  logger::init_logger("debug");

  std::vector<std::string> game_states;
  if(result.count("subgame")){
    std::filesystem::path dir(BULLDOG_DIR_DATA_LAB);
    std::filesystem::path file(result["subgame"].as<std::string>());
    if (!std::filesystem::exists(dir/file)) {
      logger::critical("missing %s", dir / file);
    }
    std::ifstream in((dir/file).c_str());
    if(!in){
      logger::critical("cannot open file %s", dir / file);
    }
    std::string str;
    while(std::getline(in, str)){
      if(str.size()>0)
        game_states.push_back(str);
    }
    in.close();
  }
  logger::info("hands ignored, evaluating %d subgames", game_states.size());

  Game *game;
  rng_state_t rng;
  std::string seatName[MAX_PLAYERS];
  uint32_t numHands, seed;

  //Initiate Game
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path game_conf(result["game"].as<std::string>());
  FILE *file = fopen((dir / game_conf).c_str(), "r");
  if (file == nullptr) {
    logger::critical("Failed to find file %s", (dir / game_conf));
  }
  game = readGame(file);
  if (game == nullptr) {
    logger::critical("Failed to read content of game file %s", (dir / game_conf));
  }

  std::vector<std::string> player_names = result["players"].as<std::vector<std::string>>();
  if (player_names.size() < game->numPlayers) {
    logger::critical("insufficient number of player names");
  }

  for (int i = 0; i < game->numPlayers; i++) {
    seatName[i] = player_names[i];
  }
  numHands = result["hands"].as<int>();
  seed = result["seed"].as<int>();
  init_genrand(&rng, seed);

  //Initiate Engine, hardcode all to random action
  const char *solvers[] = {
//      "engine_random_action.json",
//      "engine_random_action.json"
      "engine_dummy_blueprint.json",
      "engine_dummy_blueprint.json"
  };
  Engine *engines[game->numPlayers];
  for (int i = 0; i < game->numPlayers; i++) {
    engines[i] = new Engine(solvers[i], game);
  }

  /*
   * Assuming the server is always on the seat 1. quick hack
   */
  BulldogController server;
  server.setEndpoint("http://host_auto_ip4:8080/v1/bulldog/api");
  server.LoadDefault(solvers[1], game_conf);
  server.accept().wait();
  logger::info("listing for requests at:%s ", server.endpoint());

  BulldogSDK sdk;
  sdk.SetEndpoint("http://192.168.1.177:8080/v1/bulldog/api");
  sdk.SendHeartbeat(cLogger);
  sdk.CreateTableSession(cLogger, game_conf, TABLE_POKERSTAR_LITE_NLH2_50_100);

  MatchState state;
  double value[MAX_PLAYERS], totalValue[MAX_PLAYERS];

  //start at first hand
  unsigned int id = 0;
  if(!game_states.empty()){
    int r = readState(game_states[id].c_str(), game, &state.state);
    if(r==-1){
      logger::critical("error in parsing match state %s", game_states[id]);
    }
    dealRemainingCards(game, &rng, &state.state);
  } else{
    initState(game, id, &state.state);
    dealCards(game, &rng, &state.state);
  }

  for (int seat = 0; seat < game->numPlayers; seat++) {
    totalValue[seat] = 0.0;
  }

  /* seat 0 is player 0 in first games */
  uint8_t player0Seat = 0;
  while (true) {
    engines[0]->RefreshEngineState();
    sdk.NewHand(cLogger);

    /* play the hand */
    while (!stateFinished(&state.state)) {
      /* find the current player */
      uint8_t currentP = currentPlayer(game, &state.state);

      /* get action from current player */
      state.viewingPlayer = currentP;
      auto currentSeat = playerToSeat(game, player0Seat, currentP);
      Action action;
      /*
       * hack
       */
      if (currentSeat == 0){
        engines[currentSeat]->GetAction(&state, action, 2000);
      } else {
        sdk.GetAction(cLogger, game, state, action, 0);
;      }

//      engines[currentSeat]->GetAction(&state, 20, action);
      doAction(game, &action, &state.state);
    }
    /* get values */
    for (int p = 0; p < game->numPlayers; p++) {
      value[p] = valueOfState(game, &state.state, p);
      totalValue[playerToSeat(game, player0Seat, p)] += value[p];
    }

    //print results
    char line[MAX_LINE_LEN];
//    int c = printState(game, &state.state, MAX_LINE_LEN, line);
    std::string total, players;
    for (int i = 0; i < game->numPlayers; i++) {
      std::string str = i ? ("|" + std::to_string(value[i])) : std::to_string(value[i]);
      str.erase(str.find_last_not_of('0') + 1, std::string::npos);
      if (str.back() == '.')
        str.pop_back();
      total += str;
      players +=
          i ? ("|" + seatName[playerToSeat(game, player0Seat, i)]) : seatName[playerToSeat(game, player0Seat, i)];
    }
    logger::info("%s:%s:%s", line, total, players);
    char state_str[1024];
    printState(game, &state.state, 1024, state_str);
    logger::info("%s", state_str);
    sdk.EndHand(cLogger);

    //next hand
    id++;
    if(!game_states.empty()){
      if(id>=game_states.size()){
        break;
      }
      int r = readState(game_states[id].c_str(), game, &state.state);
      if(r==-1){
        logger::critical("unable to parse match state %s", game_states[id]);
      }
      dealRemainingCards(game, &rng, &state.state);
    } else{
      if (id >= numHands) {
        break;
      }
      /* rotate the players around the table */
      player0Seat = (player0Seat + 1) % game->numPlayers;
      initState(game, id, &state.state);
      dealCards(game, &rng, &state.state);
    }
  }

  //print final
  std::string total, players;
  for (int i = 0; i < game->numPlayers; i++) {
    std::string str = i ? ("|" + std::to_string(totalValue[i])) : std::to_string(totalValue[i]);
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    if (str.back() == '.')
      str.pop_back();
    total += str;
    players += i ? ("|" + seatName[i]) : seatName[i];
  }
  logger::info("SCORE:%s:%s", total, players);


  server.Cleanup();
  server.shutdown().wait();
  exit(EXIT_SUCCESS);
}