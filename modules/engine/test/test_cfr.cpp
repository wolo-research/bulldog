//
// Created by Isaac Zhang on 3/13/20.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <future>
#include "../src/cfr.h"
#include "../src/ag_builder.hpp"

TEST_CASE("cfr async termination") {
  logger::init_logger("debug");
  auto cfr = CFR("cfr_r0_for_test.json");
  cfr.BuildCMDPipeline();
  //build ag
  auto strategy = new Strategy();
  auto bucket_pool = new BucketPool();

  auto ag = new AbstractGame();
  std::filesystem::path dir(BULLDOG_DIR_CFG_ENG);
  std::filesystem::path file("builder_r0_for_test.json");
  AGBuilder ag_builder((dir / file), bucket_pool);
  ag_builder.Build(ag);

  strategy->SetAg(ag);
  strategy->InitMemoryAndValue(CFR_SCALAR_SOLVE);
  logger::debug("init strategy values...");
  sCFRState converge_state;
  converge_state.iteration = 10000000;
  std::atomic_bool cancellation_token = false;
  auto cfr_future = std::async(std::launch::async,
                               CFR::AsyncCfrSolving,
                               &cfr,
                               strategy,
                               nullptr,
                               &converge_state,
                               std::ref(cancellation_token),
                               0);
  auto cmd_begin = std::chrono::steady_clock::now();

  std::chrono::milliseconds span(100);
  int count = 0;
  while (cfr_future.wait_for(span) == std::future_status::timeout) {
    count++;
    if (count == 50)
      cancellation_token = true;
  }
  auto cmd_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - cmd_begin).count();
  logger::info("total time ms %d", cmd_time);
  REQUIRE(cmd_time < 5500); //give a 500ms leeway
  REQUIRE(cfr_future.get() >= CFR_SOLVING_TERMINATED_ASYNC);
}

TEST_CASE("cfr state") {
  sCFRState converge_time{100, -1, -1, -1};
  sCFRState converge_iter{-1, 100, -1, -1};
  sCFRState converge_expl{-1, -1, 100, -1};
  sCFRState converge_iter_time_expl{100, 100, 100, -1};
  sCFRState converge_window_expl{-1, -1, -1, 100};

  SECTION("assert time condition") {
    REQUIRE((sCFRState{10, -1, -1, -1} < converge_time) == true);
    REQUIRE((sCFRState{100, -1, -1, -1} < converge_time) == false);
    REQUIRE((sCFRState{1000, -1, -1, -1} < converge_time) == false);
  }

  SECTION("assert iter condition") {
    REQUIRE((sCFRState{10, 10, -1, -1} < converge_iter) == true);
    REQUIRE((sCFRState{1000, 100, -1, -1} < converge_iter) == false);
    REQUIRE((sCFRState{1000, 1000, -1, -1} < converge_iter) == false);
  }

  SECTION("assert expl condition") {
    REQUIRE((sCFRState{10, -1, 99, -1} < converge_expl) == false);
    REQUIRE((sCFRState{10, -1, 100, -1} < converge_expl) == false);
    REQUIRE((sCFRState{1000, -1, 1011, -1} < converge_expl) == true);
  }SECTION("assert all condition") {
    REQUIRE((sCFRState{100, 100, 100, -1} < converge_iter_time_expl) == false);
    REQUIRE((sCFRState{99, 99, 99, -1} < converge_iter_time_expl) == false);
    REQUIRE((sCFRState{99, 99, 101, -1} < converge_iter_time_expl) == true);
    REQUIRE((sCFRState{101, 99, 101, -1} < converge_iter_time_expl) == false);
    REQUIRE((sCFRState{99, 99, 101, -1} < converge_iter_time_expl) == true);
    REQUIRE((sCFRState{99, 101, 101, -1} < converge_iter_time_expl) == false);
    REQUIRE((sCFRState{99, 99, 101, -1} < converge_iter_time_expl) == true);
  }

  SECTION("assert expl standard deviation") {
    REQUIRE((sCFRState{100, 100, 100, 101} < converge_window_expl) == true);

    sCFRState state;
    state.addToWindow(1);
    state.addToWindow(2);
    state.addToWindow(3);
    state.addToWindow(4);
    state.addToWindow(5);
    state.addToWindow(6);
    state.addToWindow(7);
    state.addToWindow(8);
    state.addToWindow(9);
    state.addToWindow(10);
    REQUIRE(fabs(2.8722 - state.expl_std) < 0.0001f);
    state.addToWindow(15);
    REQUIRE(fabs(3.6455 - state.expl_std) < 0.0001f);
  }
}

TEST_CASE("capped new regret") {
  int floor = -214748000;
  REQUIRE(floor == CfrWorker::ClampRegret(floor + 999, -1000, floor));
}

TEST_CASE("flop board allocation") {
  int num_threads = 8;
  std::vector<Board_t> pub_bucket_flop_boards[HIER_PUB_BUCKET];
  std::vector<int> thread_board[num_threads];
  CFR::AllocateFlops(pub_bucket_flop_boards, thread_board, num_threads, 3);

  for (const auto &a : pub_bucket_flop_boards) {
    REQUIRE(!a.empty());
  }

  int thread_board_sum[num_threads];
  memset(thread_board_sum, 0, sizeof(int) * num_threads);

  int total = 0;
  int ideal = 22100 / num_threads;
  for (int i = 0; i < num_threads; i++) {
    for (auto idx : thread_board[i]) {
      thread_board_sum[i] += pub_bucket_flop_boards[idx].size();
    }
    total += thread_board_sum[i];
    printf("thread %d has %d flops\n", i, thread_board_sum[i]);
    REQUIRE(fabs(ideal - thread_board_sum[i]) / ideal <= 0.09);
  }

  REQUIRE(22100 == total);
}