//
// Created by Carmen C on 11/6/2020.
//

#include <cxxopts.hpp>
#include "strategy_io.h"

int main(int argc, char *argv[]) {
  /*command line parsing*/
  cxxopts::Options options("trainer configuration", "poker engine trainer");

  options.add_options()
      ("h,help", "Print help")
      ("m,mode", "reg / wavg", cxxopts::value<std::string>()->default_value("zipavg"))
      ("p,prefix", "file prefix", cxxopts::value<std::string>())
      ("d,disk_lookup", "disk?", cxxopts::value<bool>())
      ("c,cc", "open_3bet_4bet")
      ("s,match_state", "root state to start training from", cxxopts::value<std::string>());

  auto result = options.parse(argc, argv);

  logger::init_logger("debug");

  std::string prefix = result["prefix"].as<std::string>();

  auto strategy = new Strategy();
  auto bucket_pool = new BucketPool();
  LoadAG(strategy, prefix, bucket_pool, nullptr);

  auto mode = result["mode"].as<std::string>();
  auto calc_mode = StrategyMap[mode];
  bool disk_lookup = false;
  if (result.count("disk_lookup"))
    disk_lookup = result["disk_lookup"].as<bool>();
  LoadStrategy(strategy, calc_mode, prefix, disk_lookup);

  std::string print_name = prefix + "_" + mode;
  if (result.count("cc")) {
    strategy->InspectPreflopBets(print_name, calc_mode);
  } else {
    if (result.count("match_state") == 0)
      logger::critical("please provide match state");
    //inspect by any match state
    MatchState match_state;
    readMatchState(result["match_state"].as<std::string>().c_str(), &strategy->ag_->game_, &match_state);
    NodeMatchCondition condition;
    strategy->ag_->MapToNode(match_state.state, condition);
    //using open game just that we dont need to check the jupyter notebook code
    strategy->InspectNode(condition.matched_node_, prefix + "_" + mode + "_open_game", calc_mode);
  }

  delete strategy;
}