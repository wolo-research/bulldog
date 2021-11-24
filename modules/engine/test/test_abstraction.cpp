//
// Created by Carmen C on 27/3/2020.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <bulldog/core_util.hpp>
extern "C" {
#include <bulldog/game.h>
};
#include "../src/strategy_io.h"
#include "../src/builder.hpp"
#include "../src/action_abs.h"
#include <filesystem>

#include <pokerstove/peval/CardSetGenerators.h>
#include <pokerstove/peval/CardSet.h>

TEST_CASE("hier colex on flop determinisitc", "[abstraction]") {
  Board_t board{};
  Bucket bucket;
  bucket.LoadHierColex(&board, 1);
  REQUIRE(bucket.Size() == 1258712);
  Bucket bucket2;
  bucket2.LoadHierColex(&board, 1);
  REQUIRE(bucket.Size() == 1258712);

  for (auto [k, v] : bucket.master_map_){
    for (auto [k2, v2] : v){
      REQUIRE(bucket2.master_map_[k][k2] == v2);
    }
  }
}

TEST_CASE("test node match condition", "[abstraction]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  Game *game = nullptr;
  FILE *file = fopen((dir / "holdem.nolimit.2p.game").c_str(), "r");
  game = readGame(file);
  std::string game_feed_ref = "MATCHSTATE:0:2::r200r600c/r900:KcAs|/Tc6sQc";
  std::string game_feed_ref_with_check = "MATCHSTATE:0:2::cr600c/r900:KcAs|/Tc6sQc";
  std::string game_feed_s1 = "MATCHSTATE:0:2::r200r600c/r1200:KcAs|/Tc6sQc";
  std::string game_feed_s2 = "MATCHSTATE:0:2::r200r400c/r800:KcAs|/Tc6sQc";
  std::string game_feed_off = "MATCHSTATE:0:2::cr200r400c/r800:KcAs|/Tc6sQc";
  std::string game_feed_more_action = "MATCHSTATE:0:2::r200r600c/r900r1800r3600:KcAs|/Tc6sQc";
  /*
   * test c and raise are treated the same
   */
  auto betting_str_ref = GetBettingStr(game_feed_ref);
  auto betting_str_ref_with_check = GetBettingStr(game_feed_ref_with_check);
  REQUIRE(uiLevenshteinDistance(betting_str_ref, betting_str_ref_with_check) == 0);
  /*
   * test edit distance
   */

  auto betting_str_s1 = GetBettingStr(game_feed_s1);
  auto betting_str_s2 = GetBettingStr(game_feed_s2);
  auto betting_str_off = GetBettingStr(game_feed_off);
  auto betting_dist_s1_ref = uiLevenshteinDistance(betting_str_ref, betting_str_s1);
  auto betting_dist_s2_ref = uiLevenshteinDistance(betting_str_ref, betting_str_s2);
  auto betting_dist_off_ref = uiLevenshteinDistance(betting_str_ref, betting_str_off);
  REQUIRE(betting_dist_s1_ref == betting_dist_s2_ref);
  REQUIRE(betting_dist_s1_ref < betting_dist_off_ref);
  REQUIRE(betting_dist_s1_ref == 0);
  auto betting_str_more_action = GetBettingStr(game_feed_more_action);

  //it should be symmetrical
  auto betting_dist_more_ref = uiLevenshteinDistance(betting_str_ref, betting_str_more_action);
  logger::info("%s and %s", betting_str_more_action, betting_str_ref);
  REQUIRE(betting_dist_more_ref == 2);
  auto betting_dist_more_ref2 = uiLevenshteinDistance(betting_str_more_action, betting_str_ref);
  REQUIRE(betting_dist_more_ref2 == 2);
  /*
   * test decading
   */
  MatchState match_state_ref;
  REQUIRE(readMatchState(game_feed_ref.c_str(), game, &match_state_ref) > 0);
  MatchState match_state_s1;
  REQUIRE(readMatchState(game_feed_s1.c_str(), game, &match_state_s1) > 0);
  MatchState match_state_s2;
  REQUIRE(readMatchState(game_feed_s2.c_str(), game, &match_state_s2) > 0);
  auto decading_dist_s1 = DecayingBettingDistance(match_state_ref.state, match_state_s1.state);
  auto decading_dist_s2 = DecayingBettingDistance(match_state_ref.state, match_state_s2.state);
  REQUIRE(decading_dist_s1 < decading_dist_s2);

  /*
   * test betting dist
   */
  MatchState match_state_more_action;
  REQUIRE(readMatchState(game_feed_more_action.c_str(), game, &match_state_more_action) > 0);
  REQUIRE(SumBettingPatternDiff(&match_state_more_action.state, &match_state_ref.state) == 2);
  REQUIRE(SumBettingPatternDiff(&match_state_s1.state, &match_state_ref.state) == 0);
  REQUIRE(SumBettingPatternDiff(&match_state_s1.state, &match_state_s2.state) == 0);
  /*
   * test L2-distance
   */
  auto L2_s1 = PotL2(match_state_ref.state, match_state_s1.state);
  auto L2_s2 = PotL2(match_state_ref.state, match_state_s2.state);
  REQUIRE(L2_s1 > L2_s2);

  NodeMatchCondition n_s1{nullptr, L2_s1, betting_dist_s1_ref, decading_dist_s1};
  NodeMatchCondition n_s2{nullptr, L2_s2, betting_dist_s2_ref, decading_dist_s2};
  REQUIRE(n_s1 < n_s2);

  free(game);
}

TEST_CASE("validate bucket files", "[abstraction]") {
  Bucket bucket;
  std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);

  for (auto &p: std::filesystem::directory_iterator(dir)) {
    std::string file = p.path().filename();
    if (file.find(BUCKET_FILE_PREFIX) != std::string::npos &&
        file.find(BUCKET_CP_PREFIX) == std::string::npos) {

      std::vector<std::string> parsed_str;
      split_string(file, "_", parsed_str);

      //this if for only classic kmeans buckets, not hierarchical
      if (parsed_str.size() == 4) {
        bucket.LoadFromFile(dir / file);

        SECTION("bucket keys") {
          int card_count = std::stoi(parsed_str[2]) + std::stoi(parsed_str[3]);
          REQUIRE((int)bucket.Size() == CanonCardsetCount[card_count]);
        };

        SECTION("bucket values") {
          int buckets = std::stoi(parsed_str[1]);
          for (auto const&[key, val] : bucket.ExtractMap()) {
            REQUIRE((int)val < buckets);
          }
        }
      }
    }
  }
}

//TEST_CASE("construct pubcolex") {
//  //read public clusters
//  std::map<int, int> canon_pub_map;
//  std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
//  std::filesystem::path pub_file("hierarchical_pub_60_2_3.txt");
//  std::ifstream is(dir / pub_file, std::ios::binary);
//  std::string line;
//  std::getline(is, line);
//  while (std::getline(is, line)) {
//    int board_idx, bucket;
//    std::sscanf(line.c_str(), "%d,%d", &board_idx, &bucket);
//    canon_pub_map[board_idx] = bucket;
//  }
//  is.close();
//
//  auto canon_boards = pokerstove::createCardSet(3, pokerstove::Card::SUIT_CANONICAL);
//  std::filesystem::path dir2(BULLDOG_DIR_DATA_ABS);
//  std::filesystem::path pub_file2("hierarchical_pubcolex_60_2_3.txt");
//  std::ofstream os(dir2 / pub_file2, std::ios::binary);
//  os << "colex,bucket" << std::endl;
//  for (auto it = canon_boards.begin(); it != canon_boards.end(); it++) {
//    int idx = std::distance(canon_boards.begin(), it);
//    os << it->colex() << "," << canon_pub_map[idx] << std::endl;
//  }
//  os.close();
//
//  auto canon_boards = pokerstove::createCardSet(3, pokerstove::Card::SUIT_CANONICAL);
//  std::filesystem::path dir2(BULLDOG_DIR_DATA_ABS);
//  std::filesystem::path pub_file2("hierarchical_pubcards_60_2_3.txt");
//  std::ofstream os(dir2 / pub_file2, std::ios::binary);
//  os << "cards,bucket" << std::endl;
//  for (auto it = canon_boards.begin(); it != canon_boards.end(); it++) {
//    int idx = std::distance(canon_boards.begin(), it);
//    os << it->str() << "," << canon_pub_map[idx] << std::endl;
//  }
//  os.close();
//}

TEST_CASE("hierarchical bucket range", "[abstraction]") {
  Bucket bucket;
  bucket.LoadHierarchical("hierarchical_60_500_1");
  //don't need to go over all boards for test
  auto boards = pokerstove::createCardSet(3, pokerstove::Card::SUIT_CANONICAL);
  auto hands = pokerstove::createCardSet(2);

  for (const auto &board : boards) {
    for (const auto &hand : hands) {
      if (board.intersects(hand)) {
        continue;
      }

      auto cs = hand | board;
      auto b = bucket.Get(cs.canonize().colex(), board.canonize().colex());
      REQUIRE(b < 30000);
      REQUIRE(b >= 0);
    }
  }
}

TEST_CASE("hierarchical bucket unit", "[abstraction]") {
  Cardset flop, cardset1, cardset2;

  //preflop abstraction
  Bucket bucket_0;
  Board_t board{};
  bucket_0.LoadRangeColex(&board, 0);

  flop = CardsetFromString("");
  cardset1 = CardsetFromString("2c3c");
  cardset2 = CardsetFromString("2h3h");
  REQUIRE(bucket_0.Get(&cardset1, &flop) == bucket_0.Get(&cardset2, &flop));

  //flop abstraction
  auto bucket_pool = BucketPool();
  auto bucket_meta_1 = bucket_pool.Get("hierarchical_60_500_1", 1);

  flop = CardsetFromString("6h7h8h");
  cardset1 = CardsetFromString("6h7h8hKc2c");
  cardset2 = CardsetFromString("6h7h8hKc3c");
  REQUIRE(bucket_meta_1->bucket_.Get(&cardset1, &flop) == bucket_meta_1->bucket_.Get(&cardset2, &flop));

  flop = CardsetFromString("Kh2h3h");
  cardset1 = CardsetFromString("Kh2h3hKc2c");
  cardset2 = CardsetFromString("Kh2h3hKc3c");
  REQUIRE(bucket_meta_1->bucket_.Get(&cardset1, &flop) == bucket_meta_1->bucket_.Get(&cardset2, &flop));

  flop = CardsetFromString("2h3h4h");
  cardset1 = CardsetFromString("2h3h4hAc2d");
  cardset2 = CardsetFromString("2h3h4hAd2c");
  REQUIRE(bucket_meta_1->bucket_.Get(&cardset1, &flop) == bucket_meta_1->bucket_.Get(&cardset2, &flop));
}

//TEST_CASE("bucket speed", "[abstraction]") {
//  std::vector<std::string> CASES{
//      "4cAc8dKd2hQh2s",
//      "2c3c4c5c6c7c8c",
//      "5cAc8dKd2hQh2s",
//      "2c3c4c5c6c7c9c",
//      "6cAc8dKd2hQh2s",
//      "2c3c4c5c6c8c9c",
//      "7cAc8dKd2hQh2s",
//      "2c3c4c5c7c8c9c",
//      "8cAc8dKd2hQh2s",
//      "2c3c4c6c7c8c9c",
//      "9cAc8dKd2hQh2s",
//      "2c3c5c6c7c8c9c",
//      "TcAc8dKd2hQh2s",
//      "2c4c5c6c7c8c9c",
//      "JcAc8dKd2hQh2s",
//      "3c4c5c6c7c8c9c",
//      "2c3c4c5c6c7cTc",
//      "QcAc8dKd2hQh2s",
//      "2c3c4c5c6c8cTc",
//      "KcAc8dKd2hQh2s"
//  };
//
//  std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
//  Bucket bucket;
//
//  std::filesystem::path river_file("buckets_200_2_5.bin");
//  bucket.Load(dir / river_file);
//
//  for (const std::string &v : CASES) {
//    std::cout << "case:" << v << std::endl;
//    Cardset cardset = CardsetFromString(v);
//    auto begin = std::chrono::steady_clock::now();
//    int index = bucket.Get(&cardset);
//    auto end = std::chrono::steady_clock::now();
//    auto total_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
//    std::cout << "total: " << index << "|" << total_time_ns << std::endl;
//
//    begin = std::chrono::steady_clock::now();
//    uint64_t canon = Canonize(cardset.cards);
//    end = std::chrono::steady_clock::now();
//    total_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
//    std::cout << "canon: " << canon << "|" << total_time_ns << std::endl;
//
//    begin = std::chrono::steady_clock::now();
//    int colex = ComputeColex(canon);
//    end = std::chrono::steady_clock::now();
//    total_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
//    std::cout << "colex: " << colex << "|" << total_time_ns << std::endl;
//
//    begin = std::chrono::steady_clock::now();
//    index = bucket.Get(colex);
//    end = std::chrono::steady_clock::now();
//    total_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
//    std::cout << "index: " << index << "|" << total_time_ns << std::endl;
//    std::cout << "------------------------\n";
//  }
//}

TEST_CASE("pokerstove cardset generator") {
  REQUIRE(52 == pokerstove::createCardSet(1).size());
  REQUIRE(1326 == pokerstove::createCardSet(2).size());
  REQUIRE(22100 == pokerstove::createCardSet(3).size());

  REQUIRE(13 == pokerstove::createCardSet(1, pokerstove::Card::SUIT_CANONICAL).size());
  REQUIRE(169 == pokerstove::createCardSet(2, pokerstove::Card::SUIT_CANONICAL).size());
  REQUIRE(1755 == pokerstove::createCardSet(3, pokerstove::Card::SUIT_CANONICAL).size());

  REQUIRE(13 == pokerstove::createCardSet(1, pokerstove::Card::RANK).size());
  REQUIRE(91 == pokerstove::createCardSet(2, pokerstove::Card::RANK).size());
  REQUIRE(455 == pokerstove::createCardSet(3, pokerstove::Card::RANK).size());
}

TEST_CASE("pokerstove cardset") {
  REQUIRE(1 == pokerstove::CardSet("Ac").size());
  REQUIRE(1 == pokerstove::CardSet("qh").size());
  REQUIRE(13 == pokerstove::CardSet("2h3h4h5h6h7h8h9hThJhQhKhAh").size());
}

TEST_CASE("kmeans") {
  int labelMap[106] = {0, 326, 4276, 1840, 2529, 3442, 2559, 2700, 699, 752,
                       3459, 138, 517, 1224, 1287, 129, 1141, 1663, 4048, 3555,
                       3383, 1313, 2886, 3534, 4184, 2185, 2657, 4790, 246, 3338,
                       3305, 1861, 2654, 747, 1516, 3067, 1383, 4412, 2007, 315,
                       3343, 2559, 4041, 4276, 2677, 752, 2700, 3465, 2853, 2624,
                       1141, 164, 2706, 4141, 4795, 3243, 4184, 410, 2516, 2147,
                       2185, 4680, 89, 1663, 9, 1646, 3433, 1286, 579, 1326, 1065,
                       7, 1750, 2436, 423, 4412, 2559, 3101, 817, 2098, 4041, 2779,
                       2337, 2700, 4141, 1969, 1975, 1825, 709, 3967, 2321, 2310,
                       2147, 2853, 3322, 3962, 3695, 3783, 2, 3243, 3949, 2062, 2176,
                       579, 1231, 1704};

  std::vector<Entry> entries;
  std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
  read_entries(entries, dir / "features_hand_histo_2_4.bin");
  int size = entries.size();

  KMeans::sRawData raw_data{};
  raw_data.data_ = new double[size * HISTOGRAM_SIZE];
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < HISTOGRAM_SIZE; j++) {
      raw_data.data_[i * HISTOGRAM_SIZE + j] = entries[i].histo_[j];
    }
  }

  auto *kmeans = new KMeans(HISTOGRAM_SIZE, 5000, 1);
  kmeans->SetInitMode(KMeans::InitUniform);
  kmeans->Init(raw_data, size);
  double **means = kmeans->GetMeans();
  auto *loc_x = new double[HISTOGRAM_SIZE];
  int label = -1;

  for (int i = 0; i < 10; i++) {
    double dist = std::numeric_limits<double>::max();
    for (int j = 0; j < HISTOGRAM_SIZE; j++) {
      loc_x[j] = raw_data.data_[i * HISTOGRAM_SIZE + j];
    }
    for (int c = 0; c < 5000; c++) {
      double temp = KMeans::CalcDistance(loc_x, means[c], HISTOGRAM_SIZE);
      if (temp < dist) {
        dist = temp;
        label = c;
      }
    }
    REQUIRE(label == labelMap[i]);
  }
  delete[] raw_data.data_;
  delete[] loc_x;
  delete kmeans;
}

/*
 * all children should be sorted.
 * all sampled children should be sorted correctly.
 * action and node* are equal.
 */
void RecursiveTest(Node *root_node_) {
  if (root_node_->IsTerminal())
    return;

  int last_code = -2;
  logger::info("checking node at state :");
  root_node_->PrintState();
  for (int i = 0; i < root_node_->GetAmax(); i++) {
    auto this_code = root_node_->children[i]->GetLastActionCode();
    if (this_code <= last_code) {
      for (int i2 = 0; i2 < root_node_->GetAmax(); i2++)
        root_node_->children[i2]->PrintState();
      logger::error("wrong. action codes are sorted.");
    }
    //sorted
    REQUIRE(this_code > last_code);
    REQUIRE(this_code == root_node_->children[i]->GetLastActionCode());
    //sisbling index correct.
    REQUIRE(root_node_->children[i]->sibling_idx_ == i);
    last_code = this_code;
  }
  int count = 0;
  for (auto c : root_node_->children) {
    REQUIRE(c->sibling_idx_ == count++);
    RecursiveTest(c);
  }
}

TEST_CASE("node function test", "[abstraction]") {
  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path game_file("holdem.nolimit.2p.game");
  FILE *file = fopen((dir / game_file).c_str(), "r");
  if (file == nullptr) {
    logger::critical("failed to open game file %s", game_file);
  }
  Game *game_ = readGame(file);
  {
    //showdown
    const char *match_state = "MATCHSTATE:0:0::cr200c/cc/cc/cc:2ckd|2s3d/7sKh9s/Qd/6s";
    MatchState state;
    readMatchState(match_state, game_, &state);
    Node node{};
    node.SetState(state.state);
    REQUIRE(node.IsShowdown());
    REQUIRE(node.GetStake(0) == 200);
    REQUIRE(node.GetStake(1) == 200);
  }
  {
    //1 fold
    const char *match_state = "MATCHSTATE:0:0::cr200c/cr500f:2ckd|2s3d/7sKh9s";
    MatchState state;
    readMatchState(match_state, game_, &state);
    Node node{};
    node.SetState(state.state);
    REQUIRE(!node.IsShowdown());
    REQUIRE(node.GetStake(0) == 200);
    REQUIRE(node.GetStake(1) == -200);
    REQUIRE(node.IsTerminal());
  }
  {
    //0 fold
    const char *match_state = "MATCHSTATE:0:0::cr200c/cr500r9888f:2ckd|2s3d/7sKh9s";
    MatchState state;
    readMatchState(match_state, game_, &state);
    Node node{};
    node.SetState(state.state);
    REQUIRE(!node.IsShowdown());
    REQUIRE(node.GetStake(0) == -500);
    REQUIRE(node.GetStake(1) == 500);
    REQUIRE(node.IsTerminal());
  }
  {
    //current player
    const char *match_state = "MATCHSTATE:0:0::cr200c/cr500r9888:2ckd|2s3d/7sKh9s";
    MatchState state;
    readMatchState(match_state, game_, &state);
    Node node{};
    node.game_ = game_;
    node.SetState(state.state);
    REQUIRE(node.GetActingPlayer() == 0);
    REQUIRE(node.GetLastActionCode() == 9888);
  }
  {
    //others
    const char *match_state = "MATCHSTATE:0:0::cr200c/cc:2ckd|2s3d/7sKh9s/As";
    MatchState state;
    readMatchState(match_state, game_, &state);
    Node node{};
    node.game_ = game_;
    node.SetState(state.state);
    REQUIRE(node.FollowingChanceNode());
    REQUIRE(node.GetLastAction().type == a_call);
    REQUIRE(node.GetLastActionCode() == 0);
    REQUIRE(!node.IsTerminal());
  }
}

TEST_CASE("betting tree building test", "[abstraction]") {
  CompositeActionAbs act_abs{};

  act_abs.workers[0].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[0].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[0].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[1].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[1].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[1].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[2].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[2].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[2].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[3].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[3].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[3].raise_mode_ = POT_AFTER_CALL;

  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path game_file("holdem.nolimit.2p.game");
  FILE *file = fopen((dir / game_file).c_str(), "r");
  if (file == nullptr) {
    logger::critical("failed to open game file %s", game_file);
  }
  Game *game = readGame(file);
  State state;
  initState(game, 0, &state);
  auto root_node = act_abs.BuildBettingTree(game, state, nullptr, false);

  REQUIRE(CompositeActionAbs::IsBettingTreeValid(root_node));

  Node::DestroyBettingTree(root_node);
  free(game);
}

TEST_CASE("action abs and action_mapping", "[abstraction]") {
  logger::init_logger("trace");
  CompositeActionAbs act_abs{};
  act_abs.workers[0].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[0].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[0].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[1].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[1].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[1].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[2].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[2].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[2].raise_mode_ = POT_AFTER_CALL;

  act_abs.workers[3].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 0.5, 0, 3});
  act_abs.workers[3].raise_config_.push_back(RaiseConfig{POT_AFTER_CALL, 1, 0, 3});
  act_abs.workers[3].raise_mode_ = POT_AFTER_CALL;

  std::filesystem::path dir(BULLDOG_DIR_CFG_GAME);
  std::filesystem::path game_file("holdem.nolimit.2p.game");
  FILE *file = fopen((dir / game_file).c_str(), "r");
  if (file == nullptr) {
    logger::critical("failed to open game file %s", game_file);
  }
  Game *game_ = readGame(file);
  State state;
  initState(game_, 0, &state);
  Action forced_action;
  forced_action.type = a_raise;
  forced_action.size = 800;
  State state2 = state;
  doAction(game_, &forced_action, &state2);
  auto root_node_ = act_abs.BuildBettingTree(game_, state, &state2, false);
  RecursiveTest(root_node_);

  //Required Forced Action found.
  bool found = false;
  for (auto c : root_node_->children)
    if (c->GetLastAction().type == forced_action.type)
      if (c->GetLastAction().size == forced_action.size)
        found = true;
  REQUIRE(found);

  //test the path
  std::vector<int> path1 = {3, 2, 2, 1};
  Node *matched_node = root_node_;
  for (auto a : path1) {
    matched_node = matched_node->children[a];
    REQUIRE(matched_node->sibling_idx_ == a);
    logger::info("get child node at %d", a);
  }
  auto path = matched_node->GetPathFromRoot();
  for (int i : path1) {
    REQUIRE(path.top() == i);
    path.pop();
  }
  REQUIRE(path.empty());

  free(game_);
}