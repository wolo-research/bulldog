//
// Created by Carmen C on 23/5/2020.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_CARD_ABS_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_CARD_ABS_HPP_

#include <utility>
#include <cpprest/json.h>
#include "bucket_reader.hpp"

class CardAbs {
 public:
  CardAbs(web::json::value config, BucketPool *pool) {
    config_ = std::move(config);
    bucket_pool_ = pool;
  };

  void BuildReader(Game *game, State *state, BucketReader *reader) {
    reader->starting_round_ = state->round;
    auto reader_names = config_.as_array();
    for (int r = state->round; r < HOLDEM_MAX_ROUNDS; r++) {
      auto name = reader_names[r].as_string();
      if (name == "null") {
        logger::error("bucket abstraction is null for round %d, should not be querying it", r);
      } else if (name == "colex") {
        Board_t board{};
        BoardFromState(game, state, &board);
        reader->buckets_[r] = new Bucket();
        reader->buckets_[r]->LoadRangeColex(&board, state->round);
        reader->bucket_count_[r] = reader->buckets_[r]->Size();
      } else if (name == "hier_colex") {
        std::string final_name = name + "_" + std::to_string(r);
        if (!bucket_pool_->Has(final_name, r)) {
          logger::debug("cant find the %s, build it", final_name);
          //build it
          Board_t board{};
          BoardFromState(game, state, &board);
          BucketMeta *meta = new BucketMeta;
          meta->bucket_.LoadHierColex(&board, r);
          meta->bucket_count_ = meta->bucket_.Size();
          bucket_pool_->InsertBucket(meta, final_name, r);
        }
        auto meta = bucket_pool_->Get(final_name, r);
        reader->buckets_[r] = new Bucket();
        reader->buckets_[r] = &meta->bucket_;
        reader->bucket_count_[r] = meta->bucket_count_;

      } else if (name == "subgame_colex") {
        Board_t board{};
        BoardFromState(game, state, &board);
        reader->buckets_[r] = new Bucket();
        reader->buckets_[r]->LoadSubgameColex(&board, r);
        reader->bucket_count_[r] = reader->buckets_[r]->Size();
      } else {
        //hierarchical bucket.
        // todo: also preload the colex and hier_colex on flop to file. but it is too small. in file form helps backward compatibility
        auto meta = bucket_pool_->Get(name, r);
        reader->buckets_[r] = &meta->bucket_;
        reader->bucket_count_[r] = meta->bucket_count_;
        reader->post_flop_bucket_count_[r] = meta->post_flop_bucket_count_;
      }
    }
  }
 private:
  web::json::value config_;
  BucketPool *bucket_pool_;
};

#endif //BULLDOG_MODULES_ENGINE_SRC_CARD_ABS_HPP_
