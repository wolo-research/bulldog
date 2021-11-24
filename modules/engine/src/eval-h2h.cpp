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

uint8_t seatToPlayer(const Game *game, const uint8_t player0Seat,
                            const uint8_t seat) {
  return (seat + game->numPlayers - player0Seat) % game->numPlayers;
}

static uint8_t playerToSeat(const Game *game, const uint8_t player0Seat,
                            const uint8_t player) {
  return (player + player0Seat) % game->numPlayers;
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
      ("e,engines", "name of engines", cxxopts::value<std::vector<std::string>>())
      ("o,log_output", "final name of log output", cxxopts::value<std::string>())
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

  if(result.count("log_output")){
    std::filesystem::path dir(BULLDOG_DIR_LOG);
    std::filesystem::path filename(result["log_output"].as<std::string>());
    logger::init_logger(dir / filename, "info");
  }else{
    logger::init_logger("debug");
  }

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
  logger::debug("hands ignored, evaluating %d subgames", game_states.size());

  Game *game;
  rng_state_t rng;
  std::string seatName[MAX_PLAYERS];
  uint32_t numHands, seed;

  //Initiate Game
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path filename(result["game"].as<std::string>());
  FILE *file = fopen((dir / filename).c_str(), "r");
  if (file == nullptr) {
    logger::critical("Failed to find file %s", (dir / filename));
  }
  game = readGame(file);
  if (game == nullptr) {
    logger::critical("Failed to read content of game file %s", (dir / filename));
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
  auto engine_name = result["engines"].as<std::vector<std::string>>();
  const char *solvers[] = {
      engine_name[0].c_str(),
      engine_name[1].c_str()
  };
  Engine *engines[game->numPlayers];
  auto bucket_pool = new BucketPool();
  auto bp_pool = new StrategyPool();
  for (int i = 0; i < game->numPlayers; i++) {
    engines[i] = new Engine(solvers[i], game, bucket_pool, bp_pool);
    TableContext context;
    context.session_game = *game;
    context.table_name_ = std::to_string(i);
    engines[i]->SetTableContext(context);
  }
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
    /* play the hand */
    while (!stateFinished(&state.state)) {
      /* find the current player */
      uint8_t currentP = currentPlayer(game, &state.state);

      /* get action from current player */
      state.viewingPlayer = currentP;
      auto currentSeat = playerToSeat(game, player0Seat, currentP);
      Action action;
      //it is already normalized.
      engines[currentSeat]->GetActionBySession(state, action, 5000);
      doAction(game, &action, &state.state);
    }
    /* get values */
    for (int p = 0; p < game->numPlayers; p++) {
      value[p] = valueOfState(game, &state.state, p);
      totalValue[playerToSeat(game, player0Seat, p)] += value[p];
    }

    //print results
    char line[MAX_LINE_LEN];
    /* prepare the message */
    printState(game, &state.state, MAX_LINE_LEN, line);
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
    engines[0]->RefreshEngineState();
    engines[1]->RefreshEngineState();
//    char state_str[1024];
//    printState(game, &state.state, 1024, state_str);
//    logger::info("%s", state_str);

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
  delete bp_pool;
  delete bucket_pool;
  exit(EXIT_SUCCESS);
}