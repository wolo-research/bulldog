//
// Created by Isaac Zhang on 4/6/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_HAND_KERNEL_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_HAND_KERNEL_HPP_

#include "bucket_reader.hpp"
#include "hand_belief.h"
#include "term_eval_kernel.h"
#include <set>

struct sHandKernel {
    Bucket_t bucket_by_round_vector_idx[HOLDEM_MAX_ROUNDS][FULL_HAND_BELIEF_SIZE];
    TermEvalKernel hand_eval_kernel_;
    Board_t board_;
    int starting_round_;

    //valid_index
    int valid_index_[1081];

    sHandKernel(const Board_t &board, int starting_round) : board_(board), starting_round_(starting_round) {
        int cursor = 0;
        for (int i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
            auto high_low = FromVectorIndex(i);
            if (board_.CardCrash(high_low.first) || board_.CardCrash(high_low.second))
                continue;
            valid_index_[cursor] = i;
            cursor++;
        }
    }

    Bucket_t GetBucket(int r, int i) {
        auto b = bucket_by_round_vector_idx[r][i];
        if (b == INVALID_BUCKET) {
          logger::critical("should not have b = -1. should be filtered by pruned checking. at %d", i);
        }
        return b;
    }

    void EnrichHandKernel(BucketReader *bucket_reader) {
        for (int round = starting_round_; round < HOLDEM_MAX_ROUNDS; round++) {
            for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
                auto high_low = FromVectorIndex(i);
                //skipping crash, set as -1
                if (board_.CardCrashTillRound(high_low.first, round)
                    || board_.CardCrashTillRound(high_low.second, round)) {
                    bucket_by_round_vector_idx[round][i] = INVALID_BUCKET;
                    continue;
                }
                auto bucket = bucket_reader->GetBucketWithHighLowBoard(high_low.first, high_low.second, &board_, round);
//                if (bucket > 70000){
//                  logger::debug("b =  %d", bucket);
//                }
                bucket_by_round_vector_idx[round][i] = bucket;
            }
        }

#if DEV > 1
        //count -1 sum
        for (int round = starting_round_; round < HOLDEM_MAX_ROUNDS; round++) {
            int sum_board = HoldemSumBoardMap[round];
            int actual_count = 0;
            int supposed_count = nCk_card(52, 2) - nCk_card(52 - sum_board, 2);
            for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++)
                if (bucket_by_round_vector_idx[round][i] == INVALID_BUCKET)
                    actual_count++;
            if (actual_count != supposed_count)
                logger::critical("impossible bucket count not adding up %d != %d", actual_count, supposed_count);
        }
#endif

        //hand eval kernel
        hand_eval_kernel_.Prepare(&board_);
    };
};

#endif //BULLDOG_MODULES_ENGINE_SRC_HAND_KERNEL_HPP_
