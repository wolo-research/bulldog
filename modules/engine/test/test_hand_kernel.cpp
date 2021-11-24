//
// Created by Isaac Zhang on 3/29/20.
//

#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>
#include "../src/hand_belief.h"
#include "../src/hand_kernel.hpp"
#include "../src/cfr_worker.h"
#include <algorithm>

TEST_CASE("Range testing", "[engine]") {
  //just 0 belief
  {
    Ranges range(2);
    REQUIRE(!range.ReturnReady(false));
    range.beliefs_[0].SetAll(0.0);
    REQUIRE(!range.ReturnReady(false));
    range.beliefs_[1].SetAll(0.0);
    REQUIRE(range.ReturnReady(false));
  }
  {
    //test pruning on 0
    Ranges range(2);
    REQUIRE(!range.ReturnReady(true));
    range.beliefs_[0].SetAll(-1);
    REQUIRE(range.ReturnReady(true));
  }
  {
    //test pruning on 0
    Ranges range(2);
    REQUIRE(!range.ReturnReady(true));
    range.beliefs_[1].SetAll(-1);
    REQUIRE(range.ReturnReady(true));
  }

  //check empty return
  {
    Ranges range(2);
    range.beliefs_[0].Prune(111);
    range.beliefs_[1].Prune(111);
    Ranges new_range(&range, "empty");
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
      if (i == 111) {
        REQUIRE(new_range.beliefs_[0].IsPruned(i));
        REQUIRE(new_range.beliefs_[1].IsPruned(i));
      } else {
        REQUIRE(new_range.beliefs_[0].IsZero(i));
        REQUIRE(new_range.beliefs_[1].IsZero(i));
      }
    }
  }

}

TEST_CASE("terminal eval correctness ", "[engine]") {
  logger::init_logger("trace");
  Board_t board = {23, 46, 31, 41, 19};///7sKh9s/Qd/6s"
  auto kernel = TermEvalKernel();
  kernel.Prepare(&board);
//  term_eval.Prepare(torch::cuda::is_available() ? torch::kCUDA : torch::kCPU);
  //showdown
  {
    for (auto interval = 1; interval < 51; interval += 10) {
      logger::info("doing showdown eval check with interval %d", interval);
      auto opp_reach_prob = sHandBelief();
      for (double &i : opp_reach_prob.belief_)
        i += (double) rand() / (RAND_MAX);
      opp_reach_prob.NormalizeExcludeBoard(board);
      //test with sparse cases
      for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
        if (i % interval == 0) continue;
        opp_reach_prob.belief_[i] = 0;
      }
    }
  }
  {
    //fold
    {
      //all win
      for (auto interval = 1; interval < 51; interval += 10) {
        logger::info("doing all win eval check with interval %d", interval);
        int spent = 1;
        auto opp_reach_prob = sHandBelief();
        for (double &i : opp_reach_prob.belief_)
          i += (double) rand() / (RAND_MAX);
        opp_reach_prob.NormalizeExcludeBoard(board);
        for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
          if (i % interval == 0) continue;
          opp_reach_prob.belief_[i] = 0;
        }

        auto naive_payoff = sHandBelief();
        naive_payoff.NormalizeExcludeBoard(board);
        auto fast_payoff = sHandBelief();
        fast_payoff.NormalizeExcludeBoard(board);
        kernel.FastFoldEval(opp_reach_prob.belief_, fast_payoff.belief_, spent);
        kernel.NaiveFoldEval(opp_reach_prob.belief_, naive_payoff.belief_, spent);
        REQUIRE(naive_payoff.HandEquals(&fast_payoff));
      }
    }
    {
      //all loss
      for (auto interval = 1; interval < 51; interval += 10) {
        logger::info("doing all win eval check with interval %d", interval);
        int spent = -1;
        auto opp_reach_prob = sHandBelief();
        for (double &i : opp_reach_prob.belief_)
          i += (double) rand() / (RAND_MAX);
        opp_reach_prob.NormalizeExcludeBoard(board);
        for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
          if (i % interval == 0) continue;
          opp_reach_prob.belief_[i] = 0;
        }

        auto naive_payoff = sHandBelief();
        naive_payoff.NormalizeExcludeBoard(board);
        auto fast_payoff = sHandBelief();
        fast_payoff.NormalizeExcludeBoard(board);
        kernel.FastFoldEval(opp_reach_prob.belief_, fast_payoff.belief_, spent);
        kernel.NaiveFoldEval(opp_reach_prob.belief_, naive_payoff.belief_, spent);
        REQUIRE(naive_payoff.HandEquals(&fast_payoff));
      }
    }
  }
}

TEST_CASE("eval kernel speed bechmarking", "[engine]") {
  auto spent = 1;
  Board_t board = {44, 16, 27, 8, 1};
  auto kernel = TermEvalKernel();
  kernel.Prepare(&board);
  int iter = 10000;
  {
    auto begin = std::chrono::steady_clock::now();

    auto opp_reach_prob = sHandBelief();
    auto hsv_cached = sHandBelief();
    for (int i = 0; i < iter; i++) {
      kernel.FastShowdownEval(opp_reach_prob.belief_, hsv_cached.belief_, spent);
    }
    auto end_hand = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end_hand - begin).count();
    logger::info("fast showndown avg time %d iters || term node eval takes = %f  (microseconds)",
                 iter,
                 (double) total_time_ms / iter);
  }

  //in complete data
  {
    auto begin = std::chrono::steady_clock::now();
    auto opp_reach_prob = sHandBelief();
    auto hsv_cached = sHandBelief();
    //delete 90% data
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
      if (i % 10 == 1) continue;
      hsv_cached.belief_[i] = 0;
      opp_reach_prob.belief_[i] = 0;
    }
    for (int i = 0; i < iter; i++) {
      kernel.FastShowdownEval(opp_reach_prob.belief_, hsv_cached.belief_, spent);
    }
    auto end_hand = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end_hand - begin).count();
    logger::info("fast showndown with sparse data. avg time %d iters || term node eval takes = %f  (microseconds)",
                 iter,
                 (double) total_time_ms / iter);
  }

  {
    auto begin = std::chrono::steady_clock::now();
    auto opp_reach_prob = sHandBelief();
    auto hsv_cached = sHandBelief();
    for (int i = 0; i < iter; i++) {
      kernel.FastFoldEval(opp_reach_prob.belief_, hsv_cached.belief_, spent);
    }
    auto end_hand = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end_hand - begin).count();
    logger::info("fast folding eval avg time %d iters || term node eval takes = %f  (microseconds)",
                 iter,
                 (double) total_time_ms / iter);
  }

  {
    auto begin = std::chrono::steady_clock::now();
    auto opp_reach_prob = sHandBelief();
    auto hsv_cached = sHandBelief();
    //delete 90% data
    for (auto i = 0; i < FULL_HAND_BELIEF_SIZE; i++) {
      if (i % 10 == 1) continue;
      hsv_cached.belief_[i] = 0;
      opp_reach_prob.belief_[i] = 0;
    }
    for (int i = 0; i < iter; i++) {
      kernel.FastFoldEval(opp_reach_prob.belief_, hsv_cached.belief_, spent);
    }
    auto end_hand = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end_hand - begin).count();
    logger::info("fast folding sparse eval avg time %d iters || term node eval takes = %f  (microseconds)",
                 iter,
                 (double) total_time_ms / iter);
  }
}

TEST_CASE("hand and rank function", "[engine]") {
  sHandAndRank hr{3, 2, 1};
  sHandAndRank hr2{4, 2, 1};
  sHandAndRank hr22{4, 0, 1};
  sHandAndRank hr3{4, 2, 2};
  REQUIRE(hr.CheckCardCrash(3));
  REQUIRE(hr.CheckCardCrash(2));
  REQUIRE(hr.CardCrash(&hr2));
  REQUIRE(hr.RankEqual(&hr2));
  REQUIRE(!hr.RankHighLowSort(&hr2)); // lower high
  REQUIRE(hr.RankHighLowSort(&hr3)); // lower rank
  REQUIRE(hr2.RankHighLowSort(&hr22)); // same rank, same high, higher low
}

TEST_CASE("sample hand") {
  std::random_device g_rand_dev;
  std::mt19937 gen(g_rand_dev());
  std::uniform_int_distribution<int> distr(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
  unsigned long x = std::abs(distr(gen));
  unsigned long y = std::abs(distr(gen));
  unsigned long z = std::abs(distr(gen));

  printf("x %lu y %lu z %lu \n", x, y, z);

  Board_t board = {23, 46, 31, 41, 19};///7sKh9s/Qd/6s"
  auto hb = sHandBelief();
  hb.NormalizeExcludeBoard(board);
  int iterations = 10000000;

  std::map<int, int> m;
  for (int n = 0; n < iterations; ++n) {
    auto lucky_pick = hb.SampleHand(x, y, z);
    ++m[lucky_pick];
  }
  for (auto p : m) {
    auto sampled_distr = (p.second / (double) iterations);
    auto drift = sampled_distr / hb.belief_[p.first];
    logger::info("%d generated %d times || sampled_distr %f || real_distr %f || drift %f",
                 p.first,
                 p.second,
                 sampled_distr,
                 hb.belief_[p.first],
                 drift);
    REQUIRE(fabs(1.0 - drift) < 0.05);
  }
}