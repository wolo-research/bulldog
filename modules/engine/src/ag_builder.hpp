//
// Created by Carmen C on 23/5/2020.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_AG_BUILDER_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_AG_BUILDER_HPP_

#include "card_abs.hpp"
#include "action_abs.h"
#include "abstract_game.h"

class AGBuilder {
 public:
  Game *game_;

  virtual ~AGBuilder() {
    delete action_abs_;
    delete card_abs_;
    free(game_);
  }
  AGBuilder(const std::string &config_filepath, BucketPool *pool) {
    std::ifstream file(config_filepath);
    web::json::value config;
    if (file.good()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      config = web::json::value::parse(buffer);
      file.close();
    } else {
      logger::critical("%s is not good", config_filepath);
    }
    //storing raw config for final output
    raw_ = config;

    /* Create Game */
    std::filesystem::path game_dir(BULLDOG_DIR_CFG_GAME);
    FILE *game_file = fopen((game_dir / config.at("ag_builder").at("game_file").as_string()).c_str(), "rb");
    if (game_file == nullptr) {
      logger::critical("failed to open game file %s", game_file);
    }
    game_ = readGame(game_file);
    if (game_ == nullptr) {
      logger::critical("read game file failed%s", game_file);
    }
    /* initialize card abs */
    auto action_config = config.at("ag_builder").at("action_abs");

    card_abs_ = new CardAbs(config.at("ag_builder").at("card_abs"), pool);
    action_abs_ = new CompositeActionAbs(action_config);
    logger::debug("ag_builder configured.");
  }

  /*
   * need to set the ag.root_reach_prob separately, unless it is an empty game.
   */
  void Build(AbstractGame *ag, State *root_state = nullptr, State *forced_state = nullptr, bool depth_limited = false) {
    if (root_state == nullptr) {
      //build empty game.
      State state;
      initState(game_, 0, &state);
      ag->root_state_ = state;
    } else {
      ag->root_state_ = *root_state; //please set the root reach prob outside.
    }
    char line[1024];
    printState(game_, &ag->root_state_, 1024, line);
    logger::debug("    tree root state = %s", line);
    ag->raw_ = raw_;
    ag->game_ = *game_;
    logger::trace("ag_builder -> building betting tree...");
    ag->depth_limited_ = depth_limited;
    ag->root_node_ = action_abs_->BuildBettingTree(&ag->game_, ag->root_state_, forced_state, depth_limited);
    logger::trace("ag_builder -> building bucket reader...");
    card_abs_->BuildReader(&ag->game_, &ag->root_state_, &ag->bucket_reader_);
    Bucket_t bucket_count_by_round[4]{0, 0, 0, 0};
    ag->bucket_reader_.GetBucketCounts(bucket_count_by_round);
    logger::trace("ag_builder -> building kernel...");
    ag->BuildKernelFromRootNode(bucket_count_by_round);
    //default if in new game.
#ifdef DEV
    ag->Print();
#endif
  }
 private:
  web::json::value raw_;
  CompositeActionAbs *action_abs_;
  CardAbs *card_abs_;
};

#endif //BULLDOG_MODULES_ENGINE_SRC_AG_BUILDER_HPP_
