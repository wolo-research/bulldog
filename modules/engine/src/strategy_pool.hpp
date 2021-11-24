//
// Created by Isaac Zhang on 9/11/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_STRATEGY_POOL_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_STRATEGY_POOL_HPP_
#include "strategy.h"

struct StrategyPool {
  virtual ~StrategyPool() {
    for (auto s : pool_)
      delete s;
  }
  std::vector<Strategy *> pool_;
  bool sorted = false;

  Strategy *FindStrategy(MatchState *match_state, Game *game) {
    if (pool_.empty())
      logger::critical("no blueprint found. error");

    if (pool_.size() == 1) {
      logger::debug("picked blueprint with depth %d", GameDefaultStackDepth(&pool_.at(0)->ag_->game_));
      return pool_.at(0);
    }

    //find by the ratio. choose the ceiling one
    //sort if needed, sort by stack depth, ascending
    if (!sorted) {
      std::sort(pool_.begin(), pool_.end(),
                [](const Strategy *lhs, const Strategy *rhs) {
                  auto depth_lhs = GameDefaultStackDepth(&lhs->ag_->game_);
                  auto depth_rhs = GameDefaultStackDepth(&rhs->ag_->game_);
                  if (depth_lhs == depth_rhs)
                    logger::critical("we have blueprints of the same stack depth. not supported");
                  return depth_lhs < depth_rhs;
                });
      sorted = true;
    }
    int match_state_depth = StateStackDepth(&match_state->state, game);

    /*
     * find the blueprint with >= stack. the reason is,
     * the higher than real stack raise can be captured and translated into all in
     */
    auto it = std::find_if(pool_.begin(), pool_.end(),
                           [match_state_depth](const Strategy *strategy) {
                             int depth = GameDefaultStackDepth(&strategy->ag_->game_);
                             return depth >= match_state_depth;
                           });
    Strategy *rtn_strategy;
    if (it == pool_.end()) {
      //none match, return the highest one
      logger::debug("none blueprint with >= stackdepth found. use the highest one");
      rtn_strategy = pool_.back();
    } else {
      rtn_strategy = (*it);
    }
    logger::debug("picked blueprint with depth %d", GameDefaultStackDepth(&rtn_strategy->ag_->game_));
    return rtn_strategy;
  }
  void AddStrategy(Strategy *blueprint) {
    auto it = std::find(pool_.begin(), pool_.end(), blueprint);
    if (it == pool_.end()) {
      //add to it
      pool_.emplace_back(blueprint);
    } else {
      logger::critical("loading duplicated blueprints");
    }
  }
};
#endif //BULLDOG_MODULES_ENGINE_SRC_STRATEGY_POOL_HPP_
