//
// Created by Carmen C on 24/3/2020.
//

#include "bucket.h"
#include "node.h"
#include <bulldog/logger.hpp>

#include <cereal/archives/binary.hpp>
#include <filesystem>
#include <bulldog/card_util.h>
#include <utility>
#include <bulldog/core_util.hpp>
#include <pokerstove/peval/CardSetGenerators.h>

extern "C" {
#include <bulldog/game.h>
}

void Bucket::LoadFromFile(const std::string &ofile) {
  type_ = CLASSIC_BUCKET;

  std::filesystem::path full_path(ofile);
  std::vector<std::string> str;
  split_string(full_path.filename(), "_", str);
  if (str[0] != "buckets") {
    logger::critical("Bucket unable to load file: %s", ofile);
  }

  std::ifstream is(ofile, std::ios::binary);
  if (is.is_open()) {
    cereal::BinaryInputArchive load(is);
    load(cluster_map_);
    //kmeans_map has been stored as std::map
    for (const auto&[key, val] : cluster_map_) {
      master_map_[0][key] = val;
    }
  } else {
    logger::error("unable to open " + ofile);
    exit(EXIT_FAILURE);
  }
  is.close();
}

void Bucket::LoadRangeColex(Board_t *board, int round) {
  type_ = COLEX_BUCKET;

  std::set<Colex> colex_set;
  int bucket_index = 0;
  //for each hand, compute the colex value.
  for (Card_t low = 0; low < HOLDEM_MAX_CARDS - 1; low++) {
    for (Card_t high = low + 1; high < HOLDEM_MAX_CARDS; high++) {
      auto hand = Hand_t{high, low};
      if (board->HandCrash(hand)) continue;
      auto colex = ComputeColexFromAllCards(high, low, *board, round);
      if (colex_set.find(colex) == colex_set.end()) {
        //insert, and increment the value.
        master_map_[0][colex] = bucket_index;
        colex_set.insert(colex);
        bucket_index++;
      }
    }
  }
  logger::trace("colex bucket at round = %d || unique colex = %d || bucket count = %d",
                round, colex_set.size(), bucket_index);
}

void Bucket::Save(std::map<unsigned int, unsigned short> &entries, const std::string &ofile) {
  std::ofstream os(ofile, std::ios::binary | std::ios::trunc);
  if (os.is_open()) {
    cereal::BinaryOutputArchive archive(os);
    archive(entries);
  } else {
    logger::error("unable to open " + ofile);
  };
  os.close();
}

//todo: all these can be handled on the outside, at the bucket reader level
uint32_t Bucket::Get(unsigned long all_colex, unsigned long board_colex) {
  if (board_colex > 4294967295 && type_ == HIERARCHICAL_COLEX) {
    logger::critical("now the master map is indexed with unsigned int. so the public colex is not safe.");
  }

  if (all_colex > 4294967295) {
    logger::critical("all colex %d too large out of the range of unsigned int", all_colex);
  }

  switch (type_) {
    case HIERARCHICAL_COLEX:
    case HIERARCHICAL_BUCKET: {
      if (master_map_.empty()) {
        logger::critical("must call Bucket::LoadHierarchical first");
        return INVALID_BUCKET;
      }
      unsigned int pub_bucket;
      if (type_ == HIERARCHICAL_BUCKET) {
        pub_bucket = pub_colex_bucket_[board_colex];
      } else {
        pub_bucket = board_colex;
      }

      //check and query
      auto priv_buckets = master_map_.find(pub_bucket);
      if (priv_buckets == master_map_.end()) {
        logger::error("could not find hierarchical private bucket for [pub board colex %d]", pub_bucket);
        return INVALID_BUCKET;
      }
      auto it = priv_buckets->second.find(all_colex);
      if (it == priv_buckets->second.end()) {
        logger::error("could not find private bucket for [board colex %d] [all colex %d]", board_colex, all_colex);
        return INVALID_BUCKET;
      }
      return it->second;
    }
    case CLASSIC_BUCKET:
    case COLEX_BUCKET: {
      return master_map_[0][all_colex];
    }
    default: {
      logger::critical("bucket does not have type_d %d", type_);
      return INVALID_BUCKET;
    }
  }
}


uint32_t Bucket::Get(Cardset *all_cards, Cardset *board_cards) {
  return Get(ComputeColex(Canonize(all_cards->cards)), ComputeColex(Canonize(board_cards->cards)));
}

uint32_t Bucket::Size() {
  if (type_ == HIERARCHICAL_COLEX) {
    //it is alright, because we never use this for subgame solving for flop
    int sum = 0;
    for (auto[key, val] : master_map_) {
//      logger::debug("ket  = %d", key);
      sum += val.size();
    }
    logger::trace("hierarchical colex with iso flop = %d total = %d", master_map_.size(), sum);
    return sum;
  }
  return master_map_[0].size();
}

std::unordered_map<unsigned int, uint32_t> Bucket::ExtractMap() {
  return master_map_[0];
}

void Bucket::LoadHierarchicalPublic() {
  //find public bucket for flop
  std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
  std::filesystem::path pub_file("hierarchical_pubcolex_60_2_3.txt");
  std::ifstream pub_is(dir / pub_file, std::ios::binary);
  if (pub_is.is_open()) {
    std::string line;
    std::getline(pub_is, line);
    while (std::getline(pub_is, line)) {
      int board_idx, bucket;
      std::sscanf(line.c_str(), "%d,%d", &board_idx, &bucket);
      pub_colex_bucket_[board_idx] = bucket;
      assert(bucket < 60);
    }
  } else {
    logger::critical("unable to open %s", dir / pub_file);
  };
  pub_is.close();
}

void Bucket::LoadHierarchical(std::string name) {
  //name: hierarchical_60_500_1
  std::vector<std::string> parsed_str;
  split_string(std::move(name), "_", parsed_str);
  unsigned int round = std::atoi(parsed_str[3].c_str());
  unsigned int pub_bucket_count = std::atoi(parsed_str[1].c_str());
  unsigned int priv_bucket_count = std::atoi(parsed_str[2].c_str());

  type_ = HIERARCHICAL_BUCKET;
  //load hierarchical only works after flop
  assert(round > 0);
  std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);

  LoadHierarchicalPublic();

#if DEV > 1
  Bucket_t total_bucket_count = priv_bucket_count * pub_bucket_count;
  std::vector<bool> bucket_tally;
  bucket_tally.reserve(total_bucket_count);
  for (Bucket_t i = 0; i < total_bucket_count; i++)
    bucket_tally.emplace_back(false);
#endif

  for (unsigned int bucket = 0; bucket < pub_bucket_count; bucket++) {
    //regular bucket load
    std::filesystem::path
        priv_file("buckets_" + parsed_str[1] + "_" + parsed_str[2] + "_2_" + std::to_string(round + 2) + "_" +
        std::to_string(bucket) + ".bin");
    std::ifstream priv_is(dir / priv_file, std::ios::binary);
    if (priv_is.is_open()) {
      cereal::BinaryInputArchive load(priv_is);
      load(cluster_map_);
      logger::debug("loaded bucket file %s of size %d", priv_file, cluster_map_.size());
    } else {
      logger::critical("unable to open %s", dir / priv_file);
    }
    priv_is.close();
    //post process to assign bucket into range of 0-29999
    for (auto const&[key, val] : cluster_map_) {
      assert(val < priv_bucket_count);
      master_map_[bucket][key] = (bucket * priv_bucket_count) + val; //it > 2^16
#if DEV > 1
      bucket_tally[master_map_[bucket][key]] = true;
#endif
    }
    cluster_map_.clear();
  }
#if DEV > 1
  unsigned int empty_bucket_count = 0;
  for (unsigned int i = 0; i < total_bucket_count; i++) {
    if (!bucket_tally[i]) {
      empty_bucket_count++;
    }
  }
  logger::debug("%d/%d hierarchical buckets in round %d are empty!", empty_bucket_count, total_bucket_count, round);
#endif
}

unsigned int Bucket::GetPublicBucket(unsigned int pub_colex) {
  return pub_colex_bucket_[pub_colex];
}

/*
 * it should be board + 2, e.g. 3+2 for flop
 */
void Bucket::LoadHierColex(Board_t *board, uint8_t r) {
  if (r > HOLDEM_ROUND_RIVER)
    logger::critical("round %d does not exist in holdem", r);
  if (r == HOLDEM_ROUND_PREFLOP)
    logger::critical("does not support board hand colex for preflop", r);
  type_ = HIERARCHICAL_COLEX;
  int board_count = r == 1 ? 3 :
                    r == 2 ? 4 : 5;
  auto iso_board_combo = pokerstove::createCardSet(board_count, pokerstove::Card::SUIT_CANONICAL);
  auto raw_hand_combo = pokerstove::createCardSet(2);
  auto raw_board_combo = pokerstove::createCardSet(board_count);
  int iso_board_cursor = 0;
  int bucket_idx_cursor = 0;
  for (const auto &iso_board : iso_board_combo) {
    auto eval_board_colex = iso_board.colex();
    for (const auto &raw_board : raw_board_combo) {
      if (raw_board.canonize().colex() != eval_board_colex)
        continue;
      //the raw board belongs to the colex board
      for (const auto &raw_hand : raw_hand_combo) {
        if (raw_hand.intersects(raw_board))
          continue;
        //now the hands are valid.
        auto hand_board = pokerstove::CardSet(raw_hand.str() + raw_board.str());
        Colex all_colex = hand_board.canonize().colex();
        //skip duplicated entires
        if (master_map_[eval_board_colex].find(all_colex) == master_map_[eval_board_colex].end()) {
          master_map_[eval_board_colex][all_colex] = bucket_idx_cursor;
          bucket_idx_cursor++;
        }
      }
    }
    iso_board_cursor++;
  }
//  logger::debug("total [%d board colex] [%d hier colex] buckets", iso_board_cursor, bucket_idx_cursor);
}

void Bucket::LoadSubgameColex(Board_t *board, int round) {
  if (round != HOLDEM_ROUND_RIVER)
    logger::critical("subgame colex only support river. other rounds would be too big");

  type_ = HIERARCHICAL_COLEX;

//  auto cmd_begin = std::chrono::steady_clock::now();
  int bucket_idx_cursor = 0;
  for (Card_t c = 0; c < HOLDEM_MAX_CARDS; c++) {
    if (board->CardCrash(c)) continue;
    //use a local board
    Board_t local_board = *board;
    local_board.cards[4] = c;

    auto board_set = emptyCardset();
    for (unsigned char card : local_board.cards) {
      AddCardTToCardset(&board_set, card);
    }
    auto board_colex = ComputeColex(Canonize(board_set.cards));
    //for each hand, compute the colex value.
    for (Card_t low = 0; low < HOLDEM_MAX_CARDS - 1; low++) {
      for (Card_t high = low + 1; high < HOLDEM_MAX_CARDS; high++) {
        auto hand = Hand_t{high, low};
        if (local_board.HandCrash(hand)) continue;
        auto full_colex = ComputeColexFromAllCards(high, low, local_board, round);
        if (master_map_[board_colex].find(full_colex) == master_map_[board_colex].end()) {
          //insert, and increment the value.
          master_map_[board_colex][full_colex] = bucket_idx_cursor;
          bucket_idx_cursor++;
        }
      }
    }
  }
//  auto cmd_time =
//      std::chrono::duration_cast<std::chrono::milliseconds>(
//          std::chrono::steady_clock::now() - cmd_begin).count();
//  logger::debug("generate subgame colex takes %d ms", cmd_time);
}
