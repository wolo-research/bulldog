//
// Created by Carmen C on 11/6/2020.
//

#include <cxxopts.hpp>
#include "strategy_io.h"
#include "action_abs.h"

int main(int argc, char *argv[]) {
  /*command line parsing*/
  cxxopts::Options options("trainer configuration", "poker engine trainer");

  options.add_options()
      ("h,help", "Print help")
      ("b,builder", "builder_file", cxxopts::value<std::string>())
      ("t,buckets", "bucket counts", cxxopts::value<std::vector<int>>());

  auto result = options.parse(argc, argv);

  if (result.count("builder") == 0)
    logger::info("please provide builder file");
  if (result.count("buckets") == 0)
    logger::info("please provide bucket count");

  logger::init_logger("debug");
  std::filesystem::path dir(BULLDOG_DIR_CFG_ENG);
  std::filesystem::path ag_filename(result["builder"].as<std::string>());
  std::ifstream ag_file(dir / ag_filename);

  CompositeActionAbs *act_abs = nullptr;
  web::json::value config;
  if (ag_file.good()) {
    std::stringstream buffer;
    buffer << ag_file.rdbuf();
    config = web::json::value::parse(buffer);
    ag_file.close();
    //build
    act_abs = new CompositeActionAbs(config.at("ag_builder").at("action_abs"));
  }

  std::filesystem::path game_dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path game_file(config.at("ag_builder").at("game_file").as_string());
  FILE *file = fopen((game_dir / game_file).c_str(), "r");
  if (file == nullptr) {
    logger::critical("failed to open game file %s", game_file);
  }
  Game *game_ = readGame(file);
  State state;
  initState(game_, 0, &state);
  auto root_node_ = act_abs->BuildBettingTree(game_, state, nullptr, false);

  auto buckets = result["buckets"].as<std::vector<int>>();
  uint32_t bucket_count_by_round[4];
  for (int r = 0; r < 4; r++) {
    bucket_count_by_round[r] = (uint32_t) buckets[r];
  }

  sRNBAKernel kernel_;
  kernel_.BuildInternal(root_node_, bucket_count_by_round, false);
  kernel_.Print();

  //test node mapping

  delete act_abs;
  free(game_);
}