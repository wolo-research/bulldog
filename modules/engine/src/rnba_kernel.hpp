//
// Created by Isaac Zhang on 3/23/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_RNBA_KERNEL_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_RNBA_KERNEL_HPP_

#include <cmath>
#include <bulldog/logger.hpp>
#include "node.h"

static const int max_rounds_ = 4;
using RNBA = uint64_t;
using RnbaTuple = std::tuple<Round_t, Node_t, Bucket_t, Action_t>;
using RnbaMaxTuple = std::tuple<Round_t, Node_t, Bucket_t, Action_t>;

struct sRNBAKernel {
  //meta. param for computing internal indexing
  //can use http://www.almostinfinite.com/memtrack.html for profiling the memory usage.

  Action_t **amax_by_rn_ = new Action_t *[max_rounds_];
  RNBA **node_starting_index_by_round_ = new RNBA *[max_rounds_];

  RNBA action_total_count_by_round_[4]{0, 0, 0, 0};
  RNBA round_index_0_[max_rounds_]{0, 0, 0, 0};
  RNBA max_index_ = 0;
  Node_t nmax_by_r_[4]{0, 0, 0, 0};
  Bucket_t bmax_by_r_[max_rounds_]{0, 0, 0, 0};

  void CountInteral_r(Node *current_node) {
    if (current_node->IsTerminal())
      return;
    if (current_node->IsLeafNode())
      return;

    int round = current_node->GetRound();
    int size = current_node->children.size();
    Node_t node_idx = current_node->GetN();
    if (node_idx > std::numeric_limits<Node_t>::max()) {
      logger::critical("rnba kernel node index overflow");
    }

    nmax_by_r_[round] += 1;

    for (int i = 0; i < size; i++) {
      //recurse down
      CountInteral_r(current_node->children.at(i));
    }
  }

  void BuildInternal(Node *root_node, Bucket_t *bucket_counts, bool depth_limited) {
    int max_round = depth_limited ? root_node->GetRound() + 1 : max_rounds_;
    for (int r = root_node->GetRound(); r < max_round; r++) {
      if (bucket_counts[r] == 0) {
        logger::warn("bucket index at round (%d) = 0. error except in subgame solving", r);
      }
      if (bucket_counts[r] > std::numeric_limits<Bucket_t>::max()) {
        logger::critical("rnba kernel bucket overflow");
      }
      bmax_by_r_[r] = bucket_counts[r];
    }

    CountInteral_r(root_node);

    //allocate 4 levels anyway or the clean up would have problem
    for (int r = 0; r < max_rounds_; r++) {
      amax_by_rn_[r] = new Action_t[nmax_by_r_[r]];
      node_starting_index_by_round_[r] = new RNBA[nmax_by_r_[r]];
    }

    BuildInternal_r(root_node);

    for (int i = 0; i < max_rounds_; i++) {
      if (i < 3) {
        round_index_0_[i + 1] = round_index_0_[i]
            + bmax_by_r_[i] * action_total_count_by_round_[i];
      }
    }

    RNBA max_index_lean =
        round_index_0_[3] +
            bmax_by_r_[3] * action_total_count_by_round_[3];

    if (max_index_lean > (pow(2, 40))) {
      logger::warn("kernel index overflow > 2 ^ 40. enlarge it!");
    }

    max_index_ = max_index_lean;
  };

  /**
   * as all the node_idx, and incremented by node by round. So it is natively subgame-supported.
   * @param current_node
   */
  void BuildInternal_r(Node *current_node) {
    if (current_node->IsTerminal())
      return;
    if (current_node->IsLeafNode())
      return;

    int round = current_node->GetRound();
    int size = current_node->children.size();
    auto node_idx = current_node->GetN();

    //common
    amax_by_rn_[round][node_idx] = size;
//    logger::debug(" r = %d, n = %d", round, node_idx);
    node_starting_index_by_round_[round][node_idx] = action_total_count_by_round_[round];
    action_total_count_by_round_[round] += size;

    for (int i = 0; i < size; i++) {
      //recurse down
      BuildInternal_r(current_node->children.at(i));
    }
  };

  void Print() {
    {
      int size = 16; // assuming using pure strategy, 2 * 8 bytes
      auto m = (double) (max_index_ * size / (1024.0 * 1024.0));
      logger::breaker();
      logger::debug("VECTOR: entries = %d || bytes/entry %d || memory = %f (mb)", max_index_, size, m);
    }
    {
      int size = 8; // assuming using pure strategy, 2 * 4 bytes
      auto m = (double) (max_index_ * size / (1024.0 * 1024.0));
      logger::debug("SCALAR: entries = %d || bytes/entry %d || memory = %f (mb) || + zipavg = %f (mb)", max_index_, size, m, m * 1.125);
    }
    for (int i = 0; i < 4; i++)
      logger::debug(
          "round %d || bucket = %d || action_max_per_node = %d || node = %d || effective_total = %d || lean_round_beginning = %d",
          i,
          bmax_by_r_[i],
          action_total_count_by_round_[i],
          nmax_by_r_[i],
          bmax_by_r_[i] * action_total_count_by_round_[i],
          round_index_0_[i]);
    logger::breaker();
  }

  RNBA MaxIndex() const {
    return max_index_;
  }

  /**
   * hash methods, for kernel_index
   * use unsigned value so that we dont need to check < 0;
   */
  inline RNBA hash_rnba(uint8_t r, Node_t n, Bucket_t b, uint8_t a) {
#if DEV > 1
    auto invalid = r >= 4 || n >= nmax_by_r_[r] || b >= bmax_by_r_[r]
        || a >= amax_by_rn_[r][n];
    if (invalid)
      logger::critical("rnba kernel overflow || r = %d (< %d) || n = %d (< %d)|| b = %d (< %d)|| a = %d (< %d)",
                             r, max_rounds_,
                             n, nmax_by_r_[r],
                             b, bmax_by_r_[r],
                             a, amax_by_rn_[r][n]);
#endif
    return round_index_0_[r]
        + bmax_by_r_[r] * node_starting_index_by_round_[r][n]
        + b * amax_by_rn_[r][n]
        + a;
  }
  virtual ~sRNBAKernel() {
    for (int r = 0; r < max_rounds_; r++) {
      if (nmax_by_r_[r] > 0) {
        //for depth limit case u may not have it in later rounds
        delete[] amax_by_rn_[r];
        delete[] node_starting_index_by_round_[r];
      }
    }
    delete[] amax_by_rn_;
    delete[] node_starting_index_by_round_;
  }
};

#endif //BULLDOG_MODULES_ENGINE_SRC_RNBA_KERNEL_HPP_
