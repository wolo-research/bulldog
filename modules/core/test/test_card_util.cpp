//
// Created by Carmen C on 27/3/2020.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <bulldog/card_util.h>
extern "C" {
#include <bulldog/game.h>
};

#include <filesystem>
#include "pokerstove/peval/HoldemHandEvaluator.h"

TEST_CASE("colex set by board") {
  Board_t board;
  auto colex_set = GetCannoSetByBoard(&board, 0);
  REQUIRE(colex_set.size() == 169);
}
//todo: merge these by types
TEST_CASE("cards_to_string") {
  REQUIRE(CardsToString(CardsetFromString("2c3c4c5c6c7c").cards) == "2c3c4c5c6c7c");
  REQUIRE(CardsToString(CardsetFromString("2c3d4c5s6c7h").cards) == "2c4c6c3d7h5s");
  REQUIRE(CardsToString(CardsetFromString("AcAd").cards) == "AcAd");
  REQUIRE(CardsToString(CardsetFromString("5c7cTcKc").cards) == "5c7cTcKc");
  REQUIRE(CardsToString(CardsetFromString("KcKd9h").cards) == "KcKd9h");
}

TEST_CASE("canonize") {
  REQUIRE("2c3c" == CardsToString(Canonize(CardsetFromString("2c3c").cards)));
  REQUIRE("2c3c" == CardsToString(Canonize(CardsetFromString("2s3s").cards)));
  REQUIRE("4c3d2h" == CardsToString(Canonize(CardsetFromString("2s3h4c").cards)));
}

TEST_CASE("colex") {
  REQUIRE(0 == ComputeColex(CardsetFromString("2c3c").cards));
  REQUIRE(218 == ComputeColex(CardsetFromString("TcTd").cards));
  REQUIRE(312 == ComputeColex(CardsetFromString("AcAd").cards));
  REQUIRE(6225 == ComputeColex(CardsetFromString("QcJdTh").cards));
  REQUIRE(749 == ComputeColex(CardsetFromString("5cAc6d").cards));
  REQUIRE(1377 == ComputeColex(CardsetFromString("4cQcTd").cards));

  REQUIRE(ComputeColex(CardsetFromString("4c3d2h").cards) == ComputeColex(Canonize(CardsetFromString("2s3h4c").cards)));
  REQUIRE(ComputeColex(CardsetFromString("2c3c").cards) == ComputeColex(Canonize(CardsetFromString("2s3s").cards)));
  REQUIRE(ComputeColex(CardsetFromString("2c3c").cards) == ComputeColex(Canonize(CardsetFromString("2c3c").cards)));
  REQUIRE(ComputeColex(CardsetFromString("4c2d").cards) == ComputeColex(Canonize(CardsetFromString("2c4d").cards)));
}

TEST_CASE("rank hand") {
//  Cardset c = CardsetFromString("3c4c5c6c7c6h7h");
  auto begin = std::chrono::steady_clock::now();
//  int rank = rankCardset(c);
  auto end = std::chrono::steady_clock::now();
  auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
  std::cout << "total time acpc: " << total_time << std::endl;

  pokerstove::HoldemHandEvaluator heval;
  pokerstove::CardSet hand("6h7h");
  pokerstove::CardSet board("3c4c5c6c7c");
  begin = std::chrono::steady_clock::now();
//  pokerstove::PokerEvaluation eval = heval.evaluateRanks(hand, board);
//  int test = eval.showdownCode();
  end = std::chrono::steady_clock::now();
  total_time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
  std::cout << "total time pokerstove: " << total_time << std::endl;
}

TEST_CASE("random pick speed benchmarking") {
  int iterations = 10000;
  int avg_size = 6;
  float avg[6] = {0.2, 0.3, 0, 0.0, 0.35, 0.15};

  {
    std::random_device g_rand_dev;
    std::mt19937 gen;
    gen.seed(g_rand_dev());
    std::uniform_int_distribution<int> distr(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    unsigned long x = std::abs(distr(gen));
    unsigned long y = std::abs(distr(gen));
    unsigned long z = std::abs(distr(gen));

    std::map<int, int> m;
    auto begin = std::chrono::steady_clock::now();
    for (int n = 0; n < iterations; ++n) {
      auto lucky_pick = RndXorShift<float>(avg, avg_size, x, y, z, (1 << 16));
      ++m[lucky_pick];
    }
    auto end = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    double avg_time = (double) total_time_ms / iterations;
    logger::info("fast_rnd avg time takes = %f (mcs) || repeated %d iter", avg_time, iterations);
    for (auto p : m) {
      auto sampled_distr = (p.second / (double) iterations);
      auto drift = sampled_distr / avg[p.first];
      logger::info("%d generated %d times || sampled_distr %f || real_distr %f || drift %f",
                   p.first,
                   p.second,
                   sampled_distr,
                   avg[p.first],
                   drift);
      REQUIRE(0.95 < drift);
    }
  }

  {
    std::random_device g_rand_dev;
    std::mt19937 g_generator;
    g_generator.seed(g_rand_dev());

    std::map<int, int> m;
    auto begin = std::chrono::steady_clock::now();
    for (int n = 0; n < iterations; ++n) {
      std::discrete_distribution<> distr(avg, avg + avg_size);
      int lucky_pick = distr(g_generator);
      ++m[lucky_pick];
    }
    auto end = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    double avg_time = (double) total_time_ms / iterations;
    logger::info("random_pick_seed_once avg time takes = %f (mcs) || repeated %d iter", avg_time, iterations);
    for (auto p : m) {
      auto sampled_distr = (p.second / (double) iterations);
      auto drift = sampled_distr / avg[p.first];
      logger::info("%d generated %d times || sampled_distr %f || real_distr %f || drift %f",
                   p.first,
                   p.second,
                   sampled_distr,
                   avg[p.first],
                   drift);
      REQUIRE(0.85 < drift);
    }
  }

  {
    std::map<int, int> m;
    auto begin = std::chrono::steady_clock::now();
    for (int n = 0; n < iterations; ++n) {
      int lucky_pick = RandomPick(avg, avg_size);
      ++m[lucky_pick];
    }
    auto end = std::chrono::steady_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    double avg_time = (double) total_time_ms / iterations;
    logger::info("random_pick avg time takes = %f (mcs) || repeated %d iter", avg_time, iterations);

    for (auto p : m) {
      auto sampled_distr = (p.second / (double) iterations);
      auto drift = sampled_distr / avg[p.first];
      logger::info("%d generated %d times || sampled_distr %f || real_distr %f || drift %f",
                   p.first,
                   p.second,
                   sampled_distr,
                   avg[p.first],
                   drift);
      REQUIRE(0.85 < drift);
    }
  }
}

TEST_CASE("micel") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game2 = nullptr;
  FILE *file2 = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game2 = readGame(file2);
  std::string d2 = "MATCHSTATE:0:0::cr300c/cr600r1800c/cr5000c/:2c2d|2s3d/7sKh9s/Qd/6d";
  MatchState state;
  readMatchState(d2.c_str(), game2, &state);
  SECTION("board from state") {

    Board_t board;
    BoardFromState(game2, &state.state, &board);
    REQUIRE(board.cards[0] == makeCard(5, 3, 4));
    REQUIRE(board.cards[1] == makeCard(11, 2, 4));
    REQUIRE(board.cards[2] == makeCard(7, 3, 4));
    REQUIRE(board.cards[3] == makeCard(10, 1, 4));
    REQUIRE(board.cards[4] == makeCard(4, 1, 4));
  }
  free(game2);
}
TEST_CASE("enrich cardset to round") {
  auto high = makeCard(RANK_EIGHT, SUIT_CLUB, 4);
  auto low = makeCard(RANK_SEVEN, SUIT_CLUB, 4);

  Board_t board{};
  board.cards[0] = makeCard(RANK_TWO, SUIT_CLUB, 4);
  board.cards[1] = makeCard(RANK_THREE, SUIT_CLUB, 4);
  board.cards[2] = makeCard(RANK_FOUR, SUIT_CLUB, 4);
  board.cards[3] = makeCard(RANK_FIVE, SUIT_CLUB, 4);
  board.cards[4] = makeCard(RANK_SIX, SUIT_CLUB, 4);

  auto c = emptyCardset();
  EnrichCardSetToRound(&c, high, low, &board, 0);
  REQUIRE(CardsetFromString("7c8c").cards == c.cards);

  c = emptyCardset();
  EnrichCardSetToRound(&c, high, low, &board, 1);
  REQUIRE(CardsetFromString("7c8c2c3c4c").cards == c.cards);

  c = emptyCardset();
  EnrichCardSetToRound(&c, high, low, &board, 2);
  REQUIRE(CardsetFromString("7c8c2c3c4c5c").cards == c.cards);

  c = emptyCardset();
  EnrichCardSetToRound(&c, high, low, &board, 3);
  REQUIRE(CardsetFromString("7c8c2c3c4c5c6c").cards == c.cards);

  auto flop = emptyCardset();
  AddBoardToCardSetByRound(&flop, &board, 1);
  REQUIRE(CardsetFromString("2c3c4c").cards == flop.cards);

  auto turn = emptyCardset();
  AddBoardToCardSetByRound(&turn, &board, 2);
  REQUIRE(CardsetFromString("5c").cards == turn.cards);

  auto river = emptyCardset();
  AddBoardToCardSetByRound(&river, &board, 3);
  REQUIRE(CardsetFromString("6c").cards == river.cards);
}

TEST_CASE("generate random number") {

  std::map<int, int> m;
  for (int n = 0; n < 10000; ++n) {
    auto rnd = GenRndNumber(0, 51);
    ++m[rnd];
  }

  for (auto p : m) {
    std::cout << p.first << " generated " << p.second << " times\n";
  }
}

TEST_CASE("vector idx TO/FROM/CRASH") {
  int count = 1325;
  for (auto high = 51; high >= 0; high--) {
    for (auto low = high - 1; low >= 0; low--) {
      auto v_idx = ToVectorIndex(high, low);
      REQUIRE(v_idx == count);
      count--;
      auto pair = FromVectorIndex(v_idx);
      REQUIRE(pair.first == high);
      REQUIRE(pair.second == low);
      printf("eval high = %d | low = %d | idx = %d | high = %d | low = %d \n",
             high,
             low,
             v_idx,
             pair.first,
             pair.second);
    }
  }

  //check if low crashes are detected
  for (auto my_low = 0; my_low < 52; my_low++) {
    for (auto my_high = my_low + 1; my_high < 52; my_high++) {
      for (auto opp_low = 0; opp_low < 52; opp_low++) {
        for (auto opp_high = opp_low + 1; opp_high < 52; opp_high++) {
          auto my_idx = ToVectorIndex(my_high, my_low);
          auto opp_idx = ToVectorIndex(opp_high, opp_low);
          if (my_high == opp_high
              || my_high == opp_low
              || my_low == opp_high
              || my_low == opp_low) {
            //crashes
            printf("eval crash my high = %d | my low = %d | opp high = %d | opp low = %d \n",
                   my_high,
                   my_low,
                   opp_high,
                   opp_low);
            REQUIRE(VectorIdxClash(my_idx, opp_idx));
          } else {
            //test auto switching high low
            REQUIRE(ToVectorIndex(my_low,my_high) == ToVectorIndex(my_high,my_low));
            REQUIRE(!VectorIdxClash(my_idx, opp_idx));
          }
        }
      }
    }
  }

}

TEST_CASE("board/card crash at/till round") {
  Board_t board;
  board.cards[0] = 3;
  board.cards[1] = 4;
  board.cards[2] = 5;
  board.cards[3] = 6;
  board.cards[4] = 60;
  REQUIRE(board.CardCrashAtRound(3, 1));
  REQUIRE(board.CardCrashAtRound(4, 1));
  REQUIRE(board.CardCrashTillRound(4, 1));
  REQUIRE(board.CardCrashAtRound(5, 1));
  REQUIRE(!board.CardCrashAtRound(6, 1));
  REQUIRE(board.CardCrashAtRound(6, 2));
  REQUIRE(board.CardCrashTillRound(6, 2));
  REQUIRE(!board.CardCrashTillRound(6, 1));
}

TEST_CASE("sample sequential flop board") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game = readGame(file);
  MatchState state;
  readMatchState("MATCHSTATE:0:0::cr300:2c2d|", game, &state);

  int cur_flop_idx = 0;
  std::vector<Board_t> my_flops;

  Board_t flop1 = {0, 1, 2};
  Board_t flop2 = {10, 11, 12};
  my_flops.push_back(flop1);
  my_flops.push_back(flop2);

  Board_t board{};

  //test sampleing board by rotating pre-assigned flops
  SampleSequentialFullBoard(state.state, game, board, cur_flop_idx, my_flops);
  REQUIRE(0 == board.cards[0]);
  REQUIRE(1 == board.cards[1]);
  REQUIRE(2 == board.cards[2]);
  REQUIRE(2 < board.cards[3]);
  REQUIRE(52 > board.cards[3]);
  REQUIRE(board.cards[3] != board.cards[4]);
  REQUIRE(1 == cur_flop_idx);

  //test if we hit end of my_flops, index returns to beginning
  SampleSequentialFullBoard(state.state, game, board, cur_flop_idx, my_flops);
  REQUIRE(10 == board.cards[0]);
  REQUIRE(11 == board.cards[1]);
  REQUIRE(12 == board.cards[2]);
  REQUIRE(0 == cur_flop_idx);

  //test if no flops supplied, all 5 board cards should be randomly sampled
  my_flops.clear();
  SampleSequentialFullBoard(state.state, game, board, cur_flop_idx, my_flops);
  REQUIRE(0 == cur_flop_idx);
  REQUIRE(0 <= board.cards[0]);
  REQUIRE(52 > board.cards[0]);
  REQUIRE(0 <= board.cards[1]);
  REQUIRE(52 > board.cards[1]);
  REQUIRE(0 <= board.cards[2]);
  REQUIRE(52 > board.cards[2]);
  REQUIRE(0 <= board.cards[3]);
  REQUIRE(52 > board.cards[3]);
  REQUIRE(0 <= board.cards[4]);
  REQUIRE(52 > board.cards[4]);

  free(game);
}