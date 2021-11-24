//
// Created by Carmen C on 4/4/2020.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_BUILDER_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_BUILDER_HPP_

#include "bucket.h"
#include "kmeans.h"
#include <bulldog/logger.hpp>
#include <bulldog/card_util.h>

#include <pokerstove/peval/CardSetGenerators.h>

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>

#include <set>
#include <fstream>
#include <string>
#include <map>
#include <filesystem>
#include <pokerstove/util/combinations.h>
#include <bulldog/core_util.hpp>
#include <random>

static const std::string BUCKET_FILE_PREFIX = "buckets";
static const std::string BUCKET_FILE_EXT = ".bin";
static const std::string BUCKET_CP_PREFIX = "buckets_checkpoint";
static const std::string BUCKET_CP_EXT = ".txt";
static const std::string FEATURE_PREFIX = "features";
static const std::string FEATURE_EXT = ".bin";

//flop 3 + 2 iso hier = 1258712 (1755 pub buckets);
static std::map<int, int> CanonCardsetCount{
    {1, 13},
    {2, 169},
    {3, 1755},
    {4, 16432},
    {5, 134459},
    {6, 962988},
    {7, 6009159},
};

struct Equity {
  unsigned int index_;
  double value_;

  template<class Archive>
  void serialize(Archive &ar) {
    ar(index_, value_);
  }
};

struct Entry {
  unsigned int index_;
  unsigned short histo_[HISTOGRAM_SIZE];

  template<class Archive>
  void save(Archive &ar) const {
    ar(index_, histo_);
  }

  template<class Archive>
  void load(Archive &ar) {
    ar(index_, histo_);
  }
};

static void read_entries(std::vector<Entry> &entries, const std::string &ofile) {
  std::ifstream is(ofile, std::ios::binary);
  if (is.is_open()) {
    cereal::BinaryInputArchive load(is);
    load(entries);
  } else {
    logger::error("unable to open " + ofile);
  }
  is.close();
}

static void read_equities(std::vector<Equity> &equities, const std::string &ofile) {
  std::ifstream is(ofile, std::ios::binary);
  if (is.is_open()) {
    cereal::BinaryInputArchive load(is);
    load(equities);
  } else {
    logger::error("unable to open " + ofile);
  }
  is.close();
}

class BaseBuilder {
 public:
  enum BuildMode {
    BuildNotImplemented,
    BuildKMeansEMD,
    BuildKMeansEuclidean,
    BuildColex,
    BuildHierarchical
  };

  virtual ~BaseBuilder() = default;;
  virtual void run() = 0;
 protected:
  std::map<unsigned int, unsigned short> bucket_map_;
  std::string ofilemeta_;
  std::string ifilemeta_;
  std::string ifile_;
  int num_clusters_;
  int num_threads_;
};

class ColexBuilder : public BaseBuilder {
 public:
  ~ColexBuilder() override = default;
  explicit ColexBuilder(int priv_cards_num,
                        int pub_cards_num,
                        int num_clusters) {
    ofilemeta_ =
        std::to_string(num_clusters) + "_" + std::to_string(priv_cards_num) + "_" + std::to_string(pub_cards_num);
    num_clusters_ = num_clusters;
  }
  void run() override {
    std::set<uint64_t> ret;
    static const std::string cardstrings = "2c3c4c5c6c7c8c9cTcJcQcKcAc"
                                           "2d3d4d5d6d7d8d9dTdJdQdKdAd"
                                           "2h3h4h5h6h7h8h9hThJhQhKhAh"
                                           "2s3s4s5s6s7s8s9sTsJsQsKsAs";
    for (uint8_t i = 0; i < HOLDEM_MAX_CARDS - 1; i++) {
      for (uint8_t j = i + 1; j < HOLDEM_MAX_CARDS; j++) {
        std::string cards;
        cards += cardstrings.substr(i * 2, 2);
        cards += cardstrings.substr(j * 2, 2);
        ret.insert(ComputeColex(Canonize(CardsetFromString(cards).cards)));
      }
    }
    assert(num_clusters_ == (int) ret.size());
    int index = 0;
    for (auto it = ret.begin(); it != ret.end(); it++) {
      bucket_map_[*it] = index;
      index++;
    }

    std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
    std::filesystem::path file(BUCKET_FILE_PREFIX + "_" + ofilemeta_ + BUCKET_FILE_EXT);
    logger::info("saving bucket of size %d", bucket_map_.size());
    Bucket::Save(bucket_map_, dir / file);
    logger::info("bucket saved to %s", dir / file);
  }
};

class KMeansEMDBuilder : public BaseBuilder {
 public:
  KMeansEMDBuilder(const std::string &ifilemeta,
                   int priv_cards_num,
                   int pub_cards_num,
                   int num_clusters,
                   int num_threads,
                   int max_iterr,
                   double end_error,
                   KMeans::InitMode mode) {
    ofilemeta_ =
        std::to_string(num_clusters) + "_" + std::to_string(priv_cards_num) + "_" + std::to_string(pub_cards_num);

    std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
    std::filesystem::path file
        (FEATURE_PREFIX + "_" + ifilemeta + std::to_string(priv_cards_num) + "_" + std::to_string(pub_cards_num)
             + FEATURE_EXT);
    ifile_ = dir / file;

    num_clusters_ = num_clusters;
    num_threads_ = num_threads;
    max_iterr_ = max_iterr;
    end_error_ = end_error;
    mode_ = mode;

    if (pub_cards_num >= 5 || pub_cards_num < 0) {
      logger::critical("incompatible public cards number");
    }
    if (ifilemeta.find("hand_histo") == std::string::npos) {
      logger::critical("incompatible input file %s", ifilemeta);
    }
    if (!std::filesystem::exists(ifile_)) {
      logger::critical("missing %s", ifile_);
    }
    if (num_threads_ > (int) std::thread::hardware_concurrency()) {
      logger::critical("threads > hardware availability");
    }
    if (mode_ == KMeans::InitCheckpoint) {
      std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
      std::filesystem::path file(BUCKET_CP_PREFIX + "_" + ofilemeta_ + BUCKET_CP_EXT);
      if (!std::filesystem::exists(dir / file)) {
        logger::critical("missing %s", dir / file);
      }
    }
  }
  ~KMeansEMDBuilder() override {};
  void run() override {
    KMeans::sRawData raw_data{};
    raw_data.prefix_ = ofilemeta_;
    std::vector<Entry> entries;

    read_entries(entries, ifile_);
    int size = entries.size();
    logger::info("read %d entries from %s", size, ifile_);

    raw_data.data_ = new double[size * HISTOGRAM_SIZE];
    raw_data.index_ = new unsigned int[size];
    std::map<std::vector<double>, int> unquie_keys;
    int max_cache_index = 0;
    raw_data.cache_key_ = new int[size];
    for (int i = 0; i < size; i++) {
      std::vector<double> histogram;
      raw_data.index_[i] = entries[i].index_;
      for (int j = 0; j < HISTOGRAM_SIZE; j++) {
        raw_data.data_[i * HISTOGRAM_SIZE + j] = entries[i].histo_[j];
        histogram.push_back(entries[i].histo_[j]);
      }
      auto it = unquie_keys.find(histogram);
      if (it == unquie_keys.end()) {
        //not found
        unquie_keys[histogram] = max_cache_index;
        raw_data.cache_key_[i] = max_cache_index;
        max_cache_index++;
      } else {
        raw_data.cache_key_[i] = (*it).second;
      }
    }

    //run kmeans
    auto *kmeans = new KMeans(HISTOGRAM_SIZE, num_clusters_, num_threads_);
    kmeans->SetInitMode(mode_);
    kmeans->SetMaxIterNum(max_iterr_);
    kmeans->SetCacheSize(max_cache_index);
    kmeans->SetEndError(end_error_);
    kmeans->Cluster(raw_data, size);
    kmeans->SaveBuckets(ofilemeta_);

    delete kmeans;
    delete[] raw_data.data_;
    delete[] raw_data.index_;
    delete[] raw_data.cache_key_;
  }
 private:
  int max_iterr_;
  double end_error_;
  KMeans::InitMode mode_;
};

class KMeansEuclideanBuilder : public BaseBuilder {
 public:
  KMeansEuclideanBuilder(const std::string &ifilemeta,
                         int priv_cards_num,
                         int pub_cards_num,
                         int num_clusters,
                         int num_threads,
                         int max_iterr,
                         double end_error,
                         KMeans::InitMode mode) {
    ofilemeta_ =
        std::to_string(num_clusters) + "_" + std::to_string(priv_cards_num) + "_" + std::to_string(pub_cards_num);

    std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
    std::filesystem::path file
        (FEATURE_PREFIX + "_" + ifilemeta + std::to_string(priv_cards_num) + "_" + std::to_string(pub_cards_num)
             + FEATURE_EXT);
    ifile_ = dir / file;

    num_clusters_ = num_clusters;
    num_threads_ = num_threads;
    max_iterr_ = max_iterr;
    end_error_ = end_error;
    mode_ = mode;

    if (pub_cards_num != 5) {
      logger::critical("incompatible public cards number");
    }
    if (ifilemeta.find("equity") == std::string::npos) {
      logger::critical("incompatible input file %s", ifilemeta);
    }
    if (!std::filesystem::exists(ifile_)) {
      logger::critical("missing %s", ifile_);
    }
    if (num_threads_ > (int) std::thread::hardware_concurrency()) {
      logger::critical("threads > hardware availability");
    }
    if (mode_ == KMeans::InitCheckpoint) {
      std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
      std::filesystem::path file(BUCKET_CP_PREFIX + "_" + ofilemeta_ + BUCKET_CP_EXT);
      if (!std::filesystem::exists(dir / file)) {
        logger::critical("missing %s", dir / file);
      }
    }
  }
  ~KMeansEuclideanBuilder() override {};
  void run() override {
    KMeans::sRawData raw_data{};
    raw_data.prefix_ = ofilemeta_;
    std::vector<Equity> equities;

    read_equities(equities, ifile_);
    int size = equities.size();
    logger::info("read %d entries from %s", size, ifile_);

    raw_data.data_ = new double[size];
    raw_data.index_ = new unsigned int[size];
    for (int i = 0; i < size; i++) {
      raw_data.data_[i] = equities[i].value_;
      raw_data.index_[i] = equities[i].index_;
    }
    //run kmeans
    auto *kmeans = new KMeans(1, num_clusters_, num_threads_);
    kmeans->SetInitMode(mode_);
    kmeans->SetMaxIterNum(max_iterr_);
    kmeans->SetEndError(end_error_);
    kmeans->Cluster(raw_data, size);
    kmeans->SaveBuckets(ofilemeta_);

    delete kmeans;
    delete[] raw_data.data_;
    delete[] raw_data.index_;
  }
 private:
  int max_iterr_;
  double end_error_;
  KMeans::InitMode mode_;
};

class HierarchicalBuilder : public BaseBuilder {
 public:
  ~HierarchicalBuilder() {
    for (int i = 0; i < CanonCardsetCount[num_public_]; i++) {
      delete[] transition_table_[i];
      delete[] distance_[i];
    }
    delete[] transition_table_;
    delete[] distance_;
  }
  HierarchicalBuilder(int priv_cards_num,
                      int pub_cards_num,
                      int base_num_clusters,
                      int num_pub_clusters,
                      int num_priv_clusters,
                      int num_threads,
                      int max_iterr,
                      double end_error,
                      KMeans::InitMode mode) {
    num_public_ = pub_cards_num;
    num_priv_ = priv_cards_num;
    ofilemeta_ =
        std::to_string(num_pub_clusters) + "_" + std::to_string(num_priv_clusters) + "_"
            + std::to_string(priv_cards_num) + "_" + std::to_string(pub_cards_num);
    ifilemeta_ =
        std::to_string(base_num_clusters) + "_" + std::to_string(priv_cards_num) + "_" + std::to_string(pub_cards_num);

    std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
    std::filesystem::path
        file(BUCKET_FILE_PREFIX + "_" + std::to_string(base_num_clusters) + "_" + std::to_string(priv_cards_num) + "_"
                 + std::to_string(pub_cards_num) + BUCKET_FILE_EXT);
    ifile_ = dir / file;

    num_clusters_ = num_pub_clusters;
    num_priv_clusters_ = num_priv_clusters;
    base_num_clusters_ = base_num_clusters;
    num_threads_ = num_threads;
    max_iterr_ = max_iterr;
    end_error_ = end_error;
    mode_ = mode;
    v_ = 0;
    pub_boards_ = pokerstove::createCardSet(3, pokerstove::Card::SUIT_CANONICAL);

    transition_table_ = new unsigned int *[CanonCardsetCount[num_public_]];
    for (int i = 0; i < CanonCardsetCount[num_public_]; i++) {
      transition_table_[i] = new unsigned int[base_num_clusters];
      memset(transition_table_[i], 0, sizeof(unsigned int) * base_num_clusters);
    }

    //currently only half is filled
    distance_ = new double *[CanonCardsetCount[num_public_]];
    for (int i = 0; i < CanonCardsetCount[num_public_]; i++) {
      distance_[i] = new double[CanonCardsetCount[num_public_]];
    }

    if (pub_cards_num > 5 || pub_cards_num < 0) {
      logger::critical("incompatible public cards number");
    }
    if (!std::filesystem::exists(ifile_)) {
      logger::critical("missing %s", ifile_);
    }
    if (num_threads_ > (int) std::thread::hardware_concurrency()) {
      logger::critical("threads > hardware availability");
    }
    if (mode_ == KMeans::InitCheckpoint) {
      std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
      std::filesystem::path file(BUCKET_CP_PREFIX + "_" + ofilemeta_ + BUCKET_CP_EXT);
      if (!std::filesystem::exists(dir / file)) {
        logger::critical("missing %s", dir / file);
      }
    }
  };
  void run() override {
    bool overwrite = false;
    std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
    if (num_public_ == 3) {
      std::filesystem::path dist_file("hierarchical_distance_" + ifilemeta_ + ".txt");
      if (!std::filesystem::exists(dir / dist_file) || overwrite) {
        build_transition_table();
        build_distance_table();
      } else {
        load_distance_table(dir / dist_file);
      }
      cluster_public();
    }

    std::filesystem::path pub_file("hierarchical_pub_" +
        std::to_string(num_clusters_) + "_" +
        std::to_string(num_priv_) + "_3.txt");

    std::filesystem::path
        histo_file((num_public_ == 5 ? "features_equity_" : "features_hand_histo_") +
        std::to_string(num_priv_) + "_" +
        std::to_string(num_public_) + ".bin");
    cluster_private(dir / pub_file, dir / histo_file);
  }
  void cluster_private(std::string pub_cluster_file, std::string feature_file) {
    //read public clusters
    std::map<int, std::vector<int>> pub_bucket_map;
    std::ifstream is(pub_cluster_file, std::ios::binary);
    std::string line;
    std::getline(is, line);
    while (std::getline(is, line)) {
      int board_idx, bucket;
      std::sscanf(line.c_str(), "%d,%d", &board_idx, &bucket);
      pub_bucket_map[bucket].push_back(board_idx);
    }
    is.close();
    logger::info("loaded %d clusters from %s", pub_bucket_map.size(), pub_cluster_file);

    int size;
    std::vector<Equity> equities;
    std::vector<Entry> entries;
    if (num_public_ == 5) {
      read_equities(equities, feature_file);
      size = equities.size();
      logger::info("loaded %d equities from %s", size, ifile_);
    } else {
      read_entries(entries, feature_file);
      size = entries.size();
      logger::info("loaded %d entries from %s", size, ifile_);
    }

    auto hands = pokerstove::createCardSet(num_priv_ + num_public_ - 3);
    //only consider flop.
    auto boards = pokerstove::createCardSet(3); //boards is a set
    for (auto[bucket, board_indicies] : pub_bucket_map) {
      logger::debug("finding priv+pub belonging to bucket %d (%d public canon boards)", bucket, board_indicies.size());
      //filter out entries belonging to board_indicies
      std::set<int> cluster_colex;
      for (auto idx : board_indicies) {
        auto eval_board = std::next(pub_boards_.begin(), idx);
        for (const auto &board : boards) {
          //board_ is a canonized set
          if (board.canonize().colex() != eval_board->colex())
            continue;
          for (const auto &hand : hands) {
            if (hand.intersects(board))
              continue;
            auto hand_board = pokerstove::CardSet(hand.str() + board.str());
            //it will insert duplicated hand entries. but their colex are the same and the set will filter it out.
            cluster_colex.insert(hand_board.canonize().colex());
          }
        }
      }
      size = cluster_colex.size();
      int dim = num_public_ == 5 ? 1 : HISTOGRAM_SIZE;
      logger::info("bucket %d, filtered %d priv+pub", bucket, size);

      KMeans::sRawData raw_data{};
      raw_data.prefix_ = ofilemeta_ + "_" + std::to_string(bucket);
      raw_data.data_ = new double[size * dim];
      raw_data.index_ = new unsigned int[size];
      int max_cache_index = 0;

      int i = 0;
      if (dim == 1) {
        for (auto e : equities) {
          if(cluster_colex.find(e.index_) != cluster_colex.end()) {
            //found
            raw_data.data_[i] = e.value_;
            raw_data.index_[i] = e.index_;
            i++;
          }
        }
      } else {
        std::map<std::vector<double>, int> unique_keys;
        raw_data.cache_key_ = new int[size];
        for (auto e : entries) {
          if (cluster_colex.find(e.index_) != cluster_colex.end()) {
            //found
            std::vector<double> histogram;
            raw_data.index_[i] = e.index_;
            for (int j = 0; j < dim; j++) {
              raw_data.data_[i * dim + j] = e.histo_[j];
              if (dim > 1)
                histogram.push_back(e.histo_[j]);
            }

            if (dim > 1) {
              auto it = unique_keys.find(histogram);
              if (it == unique_keys.end()) {
                //not found
                unique_keys[histogram] = max_cache_index;
                raw_data.cache_key_[i] = max_cache_index;
                max_cache_index++;
              } else {
                raw_data.cache_key_[i] = (*it).second;
              }
            }
            i++;
          }
        }
        logger::info("unique histograms: %d/%d", max_cache_index, i);
      }

      logger::info("running kmeans to cluster %d into %d private buckets", size, num_priv_clusters_);
      //run kmeans
      auto *kmeans = new KMeans(dim, num_priv_clusters_, num_threads_);
      kmeans->SetInitMode(mode_);
      kmeans->SetMaxIterNum(max_iterr_);
      kmeans->SetCacheSize(max_cache_index);
      kmeans->SetEndError(end_error_);
      kmeans->Cluster(raw_data, size);
      kmeans->SaveBuckets(ofilemeta_ + "_" + std::to_string(bucket));

      delete kmeans;
      delete[] raw_data.data_;
      delete[] raw_data.index_;
      if (dim > 1)
        delete[] raw_data.cache_key_;
    }
  }
  void save_labels(int *labels) {
    std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
    std::filesystem::path file("hierarchical_pub_" +
        std::to_string(num_clusters_) + "_" +
        std::to_string(num_priv_) + "_" +
        std::to_string(num_public_) + ".txt");
    std::ofstream os(dir / file, std::ios::binary | std::ios::trunc);
    if (os.is_open()) {
      os << "board,bucket" << std::endl;
      for (unsigned long i = 0; i < pub_boards_.size(); i++)
        os << i << "," << labels[i] << std::endl;
    } else {
      logger::error("unable to open %s", dir / file);
      exit(EXIT_FAILURE);
    }
  }
  void cluster_public() {
    int pub_states = pub_boards_.size();
    logger::info("clustering %d public states into %d clusters", pub_states, num_clusters_);
    //initialize with kmeans++
    int centroids[num_clusters_];
    //randomly initialize 0th centroid
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_int_distribution<int> distr(0, pub_states);
    centroids[0] = distr(generator);
    //compute remaining num_clusters_-1 centroids
    for (int c = 1; c < num_clusters_; c++) {
      //find points closest to centroid
      std::vector<double> dist;
      for (int i = 0; i < pub_states; i++) {
        double d = 100.0; //distance_ max value is 1.0
        //lookup distance of point from each previously selected
        // centroids and store the minimum distance
        for (int m = 0; m < c; m++) {
          int j = centroids[m];
          double temp_dist = i != j ? distance_[i < j ? i : j][j > i ? j : i] : 0;
          d = std::min(d, temp_dist);
        }
        dist.push_back(d);
      }
      //select data point with maximum distance as our next centroid
      centroids[c] = std::distance(dist.begin(), std::max_element(dist.begin(), dist.end()));
      dist.clear();
    }

    //label data
    int *labels = new int[pub_states];
    int count[num_clusters_];
    memset(count, 0, sizeof(int) * num_clusters_);
    for (int i = 0; i < pub_states; i++) {
      double dist = -1;
      for (int m = 0; m < num_clusters_; m++) {
        int j = centroids[m];
        double temp = i != j ? distance_[i < j ? i : j][j > i ? j : i] : 0;
        if (temp < dist || dist == -1) {
          dist = temp;
          labels[i] = m;
        }
      }
      count[labels[i]]++;
    }

    //cluster public abstraction
    for (int t = 0; t < max_iterr_; t++) {
      logger::info("running iteration %d", t);
      int flag = 0;
      for (int i = 0; i < pub_states; i++) {
        double clusterDistances[num_clusters_];
        memset(clusterDistances, 0, sizeof(double) * num_clusters_);
        for (int j = 0; j < pub_states; j++) {
          if (i == j) {
            continue;
          }
          clusterDistances[labels[j]] += distance_[i < j ? i : j][j > i ? j : i];
        }
        int closest_cluster =
            std::distance(clusterDistances, std::min_element(clusterDistances, clusterDistances + num_clusters_));
        logger::debug("public state %d has closest cluster %d", i, closest_cluster);
        if (closest_cluster != labels[i]) {
          labels[i] = closest_cluster;
          flag++;
        }
      }
      if (flag == 0) {
        logger::info("no clusters were changed from previous iteration.");
        break;
      }
    }
    //print labels
    logger::info("saving cluster to file");
    save_labels(labels);
    delete[] labels;
  }
  void load_distance_table(std::string dist_file) {
    std::ifstream is(dist_file, std::ios::binary);
    std::string line;
    std::getline(is, line);
    while (std::getline(is, line)) {
      int i, j;
      double d;
      std::sscanf(line.c_str(), "%d,%d,%lf", &i, &j, &d);
      distance_[i][j] = d;
    }
    is.close();
    logger::info("loaded distance table %s", dist_file);
  }
  void build_distance_table() {
    std::filesystem::path dir(BULLDOG_DIR_DATA_ABS);
    std::filesystem::path file("hierarchical_distance_" + ifilemeta_ + ".txt");
    std::ofstream os(dir / file, std::ios::binary | std::ios::trunc);
    os.precision(10);
    os << "i_index,j_index,distance" << std::endl;

    for (auto iit = pub_boards_.begin(); iit != --pub_boards_.end(); iit++) {
      int i = std::distance(pub_boards_.begin(), iit);
      for (auto jit = std::next(iit); jit != pub_boards_.end(); jit++) {
        int j = std::distance(pub_boards_.begin(), jit);

        int similarity = 0;
        for (int k = 0; k < base_num_clusters_; k++) {
          similarity += std::min(
              transition_table_[i][k],
              transition_table_[j][k]);
        }
        distance_[i][j] = (v_ - similarity) / v_;
        os << i << "," << j << "," << distance_[i][j] << std::endl;
      }
    }
    os.close();
  }
  void build_transition_table() {
    Bucket base_abs;
    base_abs.LoadFromFile(ifile_);

    auto hands = pokerstove::createCardSet(num_priv_);

    logger::info("hands to iterate %d, boards to iterate %d", hands.size(), pub_boards_.size());
    for (auto bit = pub_boards_.begin(); bit != pub_boards_.end(); bit++) {
      for (const auto &hand : hands) {
        if (bit->intersects(hand)) {
          continue;
        }
        //tally v_ only once
        if (bit == pub_boards_.begin())
          v_++;

        auto cs = hand | *bit;
        uint32_t bucket = base_abs.Get(cs.canonize().colex(), 0);

        int i = std::distance(pub_boards_.begin(), bit);
        transition_table_[i][bucket]++;
      }
      logger::info("board %d/%d", std::distance(pub_boards_.begin(), bit), pub_boards_.size());
    }
    logger::info("built transition table");
  }
 private:
  int num_public_;
  int num_priv_;
  int max_iterr_;
  int base_num_clusters_;
  int num_priv_clusters_;
  double v_;
  double end_error_;
  KMeans::InitMode mode_;

  std::set<pokerstove::CardSet> pub_boards_;

  unsigned int **transition_table_;
  double **distance_;
};

//  csvToBin("data/abs/2_4_hand_histo.csv", "data/abs/features_2_4_hand_histo.bin");
//void csvToBin(const std::string &ifile, const std::string &ofile) {
//  std::vector<Entry> entries;
//  std::fstream istream;
//  istream.open(ifile, std::ios::in);
//  std::string line, item;
//  while (getline(istream, line)) {
//    std::stringstream s(line);
//    entries.emplace_back();
//    //get index
//    getline(s, item, ',');
//    entries.back().index_ = std::stoi(item);
//
//    for (unsigned short &i : entries.back().histo_) {
//      getline(s, item, ',');
//      i = std::stoi(item);
//    }
//  }
//
//  std::ofstream os(ofile, std::ios::binary | std::ios::trunc);
//  if (os.is_open()) {
//    cereal::BinaryOutputArchive archive(os);
//    archive(entries);
//  } else {
//    logger::error("unable to open " + ofile);
//  };
//  os.close();
//  logger::info("Write:" + std::to_string(entries.size()));
//
//  std::vector<Entry> readEntries;
//  read_entries(readEntries, ofile);
//  logger::info("Read:" + std::to_string(readEntries.size()));
//}
#endif //BULLDOG_MODULES_ENGINE_SRC_BUILDER_HPP_
