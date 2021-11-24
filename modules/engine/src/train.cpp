//
// Created by Isaac Zhang on 2/25/20.
//

#include "cfr.h"
#include "ag_builder.hpp"
#include "strategy_io.h"

#include <bulldog/logger.hpp>
#include "cxxopts.hpp"

void train(const cxxopts::ParseResult &result);

int main(int argc, char *argv[]) {
  /*command line parsing*/
  cxxopts::Options options("trainer configuration", "poker engine trainer");

  options.add_options()
      ("h,help", "Print help")
      ("l,log_level", "log level", cxxopts::value<std::string>()->default_value("info"))
      ("f,log_file", "file?")
      ("c,cfr", "MUST: file to the cfr file", cxxopts::value<std::string>())
      ("b,builder", "MUST: file to the abstract game def  file", cxxopts::value<std::string>())
      ("s,sgs", "MUST: file to the sgs file", cxxopts::value<std::string>())
      ("e,exp_tag", "tag to persist the lab file", cxxopts::value<std::string>()->default_value("x"))
      ("p,load_checkpoint", "loading strategy checkpoint", cxxopts::value<std::string>())
      ("t,blueprint", "loading blueprint for depth limit solving", cxxopts::value<std::string>())
      ("m,match_state", "root state to start training from", cxxopts::value<std::string>());

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    exit(EXIT_SUCCESS);
  }

  bool check_must_opts = (result.count("builder") && result.count("cfr")) || result.count("sgs");
  if (!check_must_opts) {
    std::cout << "must have either cfr+builder | sgs conf" << std::endl;
    std::cout << options.help() << std::endl;
    exit(EXIT_SUCCESS);
  }

  train(result);
}

void train(const cxxopts::ParseResult &result) {
  //build CFR
  std::string cfr_conf;
  std::string ag_conf;
  if (result.count("sgs") == 0) {
    cfr_conf = result["cfr"].as<std::string>();
    ag_conf = result["builder"].as<std::string>();
  } else {
    //read the sgs file, and extract accordingly
    std::filesystem::path dir(BULLDOG_DIR_CFG_ENG);
    std::filesystem::path filename(result["sgs"].as<std::string>().c_str());
    std::ifstream sgs_file(dir / filename);
    if (sgs_file.is_open()){
      std::stringstream buffer;
      buffer << sgs_file.rdbuf();
      auto sgs_config = web::json::value::parse(buffer);
      cfr_conf = sgs_config.at("cfr_file").as_string();
      ag_conf = sgs_config.at("builder_file").as_string();
    }
  }

  auto cfr = CFR(cfr_conf.c_str());
  //config logger. Supports only 4 log_level right now.
  if (result.count("log_file")) {
    std::filesystem::path dir(BULLDOG_DIR_LOG);
    std::filesystem::path filename(cfr.cfr_param_.name + "_" + result["exp_tag"].as<std::string>() + "_"
                                       + std::to_string(cfr.cfr_param_.iteration) + ".log");
    logger::init_logger(dir / filename, result["log_level"].as<std::string>());
  } else {
    logger::init_logger(result["log_level"].as<std::string>());
  }
  if (cfr.cfr_param_.depth_limited)
    logger::debug("depth limit cfr [%d][reps %d]",
                  cfr.cfr_param_.depth_limited,
                  cfr.cfr_param_.depth_limited_rollout_reps_);
  cfr.BuildCMDPipeline();

  if (result.count("exp_tag")) {
    cfr.profiling_writer_.prefix_ =
        cfr.cfr_param_.name + "_" + result["exp_tag"].as<std::string>() + "_"
            + std::to_string(cfr.cfr_param_.iteration);
  } else {
    cfr.profiling_writer_.prefix_ = cfr.cfr_param_.name + "_" + std::to_string(result["max_iter"].as<int>());
  }

  //build ag
  auto strategy = new Strategy();
  auto bucket_pool = new BucketPool();
  if (result.count("load_checkpoint")) {
    auto prefix = result["load_checkpoint"].as<std::string>();
    LoadAG(strategy, prefix, bucket_pool, nullptr);
    LoadStrategy(strategy, STRATEGY_REG, prefix, false);
    LoadStrategy(strategy, STRATEGY_WAVG, prefix, false);
  } else {
    auto ag = new AbstractGame();
    std::filesystem::path dir(BULLDOG_DIR_CFG_ENG);
    std::filesystem::path file(ag_conf);
    auto cut_tail = ag_conf.substr(0, ag_conf.length() - 5);
    ag->name_ = cut_tail.substr(11, 20); //get ride of prefix "builder_r0_" and postfix ".json"
    AGBuilder ag_builder((dir / file), bucket_pool);

    if (result.count("match_state")) {
      MatchState state;
      readMatchState(result["match_state"].as<std::string>().c_str(), ag_builder.game_, &state);
      ag_builder.Build(ag, &state.state, nullptr, cfr.cfr_param_.depth_limited);
      // training with subgame is majorly used for testing the solving.
      ag->NormalizeRootReachProb();
    } else {
      ag_builder.Build(ag, nullptr, nullptr, cfr.cfr_param_.depth_limited);
    }

    strategy->SetAg(ag);
    strategy->InitMemoryAndValue(cfr.cfr_param_.cfr_mode_);
    logger::debug("init strategy values...");
  }

  sCFRState converge_state;
  auto conv_config = cfr.cfr_param_.raw_.at("cfr").at("convergence");
  converge_state.iteration =
      conv_config.has_field("max_iter") ? conv_config.at("max_iter").as_integer() : converge_state.iteration;
  converge_state.time_milli_seconds =
      conv_config.has_field("time_ms") ? conv_config.at("time_ms").as_double() : converge_state.time_milli_seconds;
  converge_state.exploitability =
      conv_config.has_field("expl") ? conv_config.at("expl").as_double() : converge_state.exploitability;
  converge_state.expl_std =
      conv_config.has_field("expl_std") ? conv_config.at("expl_std").as_double() : converge_state.expl_std;

  //solving in no blueprint mode.
  Strategy *blueprint_ = nullptr;
  if (result.count("blueprint")) {
    blueprint_ = new Strategy();
    auto prefix = result["blueprint"].as<std::string>();
    LoadAG(blueprint_, prefix, bucket_pool, nullptr);
    LoadStrategy(blueprint_, STRATEGY_ZIPAVG, prefix, false);
    //if called with multiple thread, it will have problem indexing the tree
    blueprint_->ag_->IndexBettingTree();
  }

  if (cfr.Solve(blueprint_, strategy, converge_state, std::atomic_bool(), 0) < 0) {
    logger::error("cfr solving error");
  }

  //clean up
  delete strategy;
  delete blueprint_;
  delete bucket_pool;
}
