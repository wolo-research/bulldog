//
// Created by Carmen C on 24/3/2020.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_BUCKET_H_
#define BULLDOG_MODULES_ENGINE_SRC_BUCKET_H_

#include <map>
#include <vector>
#include <cereal/access.hpp>
#include <cereal/types/map.hpp>
#include <bulldog/card_util.h>
#include <set>
extern "C" {
#include <bulldog/game.h>
}

const short HIER_PUB_BUCKET = 60;
const short HISTOGRAM_SIZE = 50;

enum BUCKET_TYPE {
  HIERARCHICAL_BUCKET,
  CLASSIC_BUCKET,
  COLEX_BUCKET,
  HIERARCHICAL_COLEX
};

class Bucket {
 public:
  BUCKET_TYPE type_;
  static void Save(std::map<unsigned int, unsigned short> &entries, const std::string &ofile);
  void LoadFromFile(const std::string &ofile);
  /*
   * bucket type
   */
  void LoadRangeColex(Board_t *board, int round);
  void LoadHierarchical(std::string name);
  void LoadHierarchicalPublic();
  void LoadHierColex(Board_t *board, uint8_t r);
  void LoadSubgameColex(Board_t *board, int r);

  uint32_t Get(unsigned long all_colex, unsigned long board_colex);
  uint32_t Get(Cardset *all_cards, Cardset *board_cards);
  unsigned int GetPublicBucket(unsigned int pub_colex);
  uint32_t Size();
  std::unordered_map<unsigned int, uint32_t> ExtractMap();

  //if not hierarchical, kmeans_map_ / colex will be stored on master_map_[0]
  //aware that the master_map should return Bucket_t type
  std::unordered_map<unsigned int, std::unordered_map<Colex, uint32_t>> master_map_;
 private:
  std::map<unsigned int, unsigned short> cluster_map_;
  std::unordered_map<Colex , unsigned short> pub_colex_bucket_;//should use unsigned int
  friend class cereal::access;
  template<class Archive>
  void serialize(Archive &ar) {
    ar(cluster_map_);
  }
};

#endif //BULLDOG_MODULES_ENGINE_SRC_BUCKET_H_
