//
// Created by Isaac Zhang on 3/10/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_BUCKET_READER_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_BUCKET_READER_HPP_

#include "bucket_pool.hpp"
#include "node.h"

class BucketReader {
 public:
  virtual ~BucketReader() {
    for (int i = starting_round_; i < 4; i++) {
      if (buckets_[i]->type_ == COLEX_BUCKET)
        delete buckets_[i];
    }
  }

  int starting_round_ = 5; // a hack to prevetn bucket reader destructor problem
  std::array<Bucket *, 4> buckets_{};
  std::array<int, 4> bucket_count_{0, 0, 0, 0};
  std::array<int, 4> post_flop_bucket_count_{0, 0, 0, 0};

  void GetBucketCounts(uint32_t *bucket_count) {
    for (int i = starting_round_; i < 4; i++) {
      if (starting_round_ > HOLDEM_ROUND_PREFLOP && buckets_[i]->type_ == HIERARCHICAL_BUCKET) {
        //for subgame solving
        bucket_count[i] = post_flop_bucket_count_[i];
      } else {
        //for blueprint training.
        bucket_count[i] = bucket_count_[i];
      }
    }
  };

  Bucket_t GetBucketWithHighLowBoard(Card_t c1, Card_t c2, Board_t *board, int round) {
    auto all_cards = emptyCardset();
    EnrichCardSetToRound(&all_cards, c1, c2, board, round);
    //build board cards
    auto board_cards = emptyCardset();
    auto bucket_type = buckets_[round]->type_;
    int board_cards_ending_round = bucket_type == HIERARCHICAL_BUCKET ? HOLDEM_ROUND_FLOP :
                       bucket_type == HIERARCHICAL_COLEX ? round : 0; //otherwise does not matter
    for (int r = 1; r <= board_cards_ending_round; r++)
      AddBoardToCardSetByRound(&board_cards, board, r);
    auto b = GetBucket(&all_cards, &board_cards, round);
    if (b == INVALID_BUCKET) {
      logger::warn("all cards %s | board cards %s", CardsToString(all_cards.cards), CardsToString(board_cards.cards));
      logger::error("no bucket for [high %d] [low %d] [round %d] [board ending round %d] [bucket type %d]",
                    c1,
                    c2,
                    round,
                    board_cards_ending_round,
                    buckets_[round]->type_);
      board->Print();
      exit(1);
    }
    return b;
  };

  Bucket_t GetBucket(Cardset *all_cards, Cardset *board, int round) {
    auto b = buckets_[round]->Get(all_cards, board);
    if (b == INVALID_BUCKET)
      return b;
    //for subgame solving use.
    if (starting_round_ > HOLDEM_ROUND_PREFLOP && buckets_[round]->type_ == HIERARCHICAL_BUCKET) {
      return b % post_flop_bucket_count_[round];
    }
    return b;
  }
};

#endif //BULLDOG_MODULES_ENGINE_SRC_BUCKET_READER_HPP_
