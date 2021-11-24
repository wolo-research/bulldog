//
// Created by Carmen C on 23/3/2020.
// Modified from https://github.com/luxiaoxun/KMeans-GMM-HMM
//

#include "kmeans.h"
#include "bucket.h"
#include "builder.hpp"

#include <filesystem>
namespace fs = std::filesystem;

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <bulldog/logger.hpp>

KMeans::KMeans(int dimNum, int clusterNum, int num_threads) {
  logger::info("initializing k-means, dimension:%d, clusters:%d, threads:%d", dimNum, clusterNum, num_threads);

  m_dimNum = dimNum;
  m_clusterNum = clusterNum;

  m_means = new double *[m_clusterNum];
  for (int i = 0; i < m_clusterNum; i++) {
    m_means[i] = new double[m_dimNum];
    memset(m_means[i], 0, sizeof(double) * m_dimNum);
  }

  m_initMode = InitRandom;
  m_maxIterNum = 100;
  m_endError = 0.001;

  cache_size_ = 0;

  num_threads_ = num_threads;
  if (num_threads > 0) {
    thread_pool_ = new pthread_t[num_threads];
    pthread_mutex_init(&mutex_, nullptr);
  }
}

KMeans::~KMeans() {
  if (num_threads_ > 0) {
    pthread_mutex_destroy(&mutex_);
    delete[] thread_pool_;
  }
  for (int i = 0; i < m_clusterNum; i++) {
    delete[] m_means[i];
  }
  delete[] m_means;
}

struct ThreadParams {
  int thread_index_;
  int num_threads_;
  int num_dim_;
  int num_clusters_;
  KMeans::sRawData raw_data;
  int size_;

  pthread_mutex_t *mutex_;
  double *currCost;
  int *counts;
  double **next_means;
  double **m_means;
  std::map<unsigned int, unsigned short> *bucket_map_;
//  std::map<std::vector<double>, std::array<double, 2>> *cache_;
  double *dist_cache_;
  int *label_cache_;
};

struct PPThreadParams {
  int thread_index;
  int num_dim;
  int cluster;
  double **m_means;
  double *data;
  double *dist;
  int size;
  int threads;
  std::map<std::vector<double>, std::array<double, 2>> *cache;
  pthread_mutex_t *mutex;
};

static void *thread_initplusplus_work(void *t_params) {
  auto *params = (PPThreadParams *) t_params;

  double sample[params->num_dim];
  for (int i = params->thread_index; i < params->size; i += params->threads) {
    double d = std::numeric_limits<double>::max();

    for (int k = 0; k < params->num_dim; k++)
      sample[k] = params->data[i * params->num_dim + k];

    std::vector<double> key(sample, sample + params->num_dim);
    bool in_map = (*params->cache).find(key) != (*params->cache).end();
    if (in_map) {
      d = (*params->cache)[key][0];
    }
    if (!in_map) {
      for (int m = 0; m < params->cluster; m++) {
        double temp = KMeans::CalcDistance(sample, params->m_means[m], params->num_dim);
        d = std::min(d, temp);
      }
      (*params->cache)[key][0] = d;
    }
    params->dist[i] = d;
  }
  delete params;
  return nullptr;
}

static void *thread_classification_work(void *t_params) {
  auto *params = (ThreadParams *) t_params;
  auto *loc_x = new double[params->num_dim_];
  double loc_curr_cost = 0;
  int *loc_counts = new int[params->num_clusters_];
  auto **loc_next_means = new double *[params->num_clusters_];
  std::map<unsigned int, unsigned short> local_map;
  int count = 0;

  //Initialize local variables
  memset(loc_counts, 0, sizeof(int) * params->num_clusters_);
  for (int i = 0; i < params->num_clusters_; i++) {
    loc_next_means[i] = new double[params->num_dim_];
    memset(loc_next_means[i], 0, sizeof(double) * params->num_dim_);
  }

  int label = -1;
  for (int i = params->thread_index_; i < params->size_; i += params->num_threads_) {
    count++;
    for (int j = 0; j < params->num_dim_; j++) {
      loc_x[j] = params->raw_data.data_[i * params->num_dim_ + j];
    }

    //get label
//    auto begin = std::chrono::steady_clock::now();
//    int hit;
    double dist = std::numeric_limits<double>::max();

    if(params->num_dim_>1){
      int key = params->raw_data.cache_key_[i];

      if (params->dist_cache_[key] != -1) {
        //found
        dist = params->dist_cache_[key];
        label = params->label_cache_[key];
      } else {
//      hit = 0;
        for (int c = 0; c < params->num_clusters_; c++) {
          double temp = KMeans::CalcDistance(loc_x, params->m_means[c], params->num_dim_);
          if (temp < dist) {
            dist = temp;
            label = c;
          }
        }
        params->dist_cache_[key] = dist;
        params->label_cache_[key] = label;
      }
    }else{ //euclidean distance
      for (int c = 0; c < params->num_clusters_; c++) {
        double temp = KMeans::CalcDistance(loc_x, params->m_means[c], params->num_dim_);
        if (temp < dist) {
          dist = temp;
          label = c;
        }
      }
    }

//    auto end = std::chrono::steady_clock::now();
//    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
//    logger::debug("hit=%d entry=%d label=%d dist=%.6f in %dms", hit, i, label, dist, total_time);

    loc_curr_cost += dist;
    loc_counts[label]++;
    local_map[params->raw_data.index_[i]] = label;
    for (int d = 0; d < params->num_dim_; d++) {
      loc_next_means[label][d] += loc_x[d];
    }
    logger::debug("classified:%d/%d", i, params->size_);
    int check = params->size_ / params->num_threads_ / 5;
    if (params->thread_index_ == 0 && count % check == 0) {
      logger::info("thread 0 classified %d entries", count);
    }
  }

  pthread_mutex_lock(params->mutex_);
  (*params->currCost) += loc_curr_cost;
  for (int i = 0; i < params->num_clusters_; i++) {
    params->counts[i] += loc_counts[i];
    for (int d = 0; d < params->num_dim_; d++) {
      params->next_means[i][d] += loc_next_means[i][d];
    }
  }
  for (std::pair<unsigned int, unsigned short> bucket : local_map) {
    (*params->bucket_map_)[bucket.first] = bucket.second;
  }
  pthread_mutex_unlock(params->mutex_);

  for (int i = 0; i < params->num_clusters_; i++) {
    delete[] loc_next_means[i];
  }
  delete[] loc_next_means;
  delete[] loc_counts;
  delete params;
  return nullptr;
}

void KMeans::Cluster(const sRawData &raw_data, int N) {
  int size = 0;
  size = N;

  assert(size >= m_clusterNum);

  // Initialize model
  Init(raw_data, N);

  dist_cache_ = new double[cache_size_];
  label_cache_ = new int[cache_size_];
  for (int i = 0; i < cache_size_; i++) {
    dist_cache_[i] = -1;
    label_cache_[i] = -1;
  }

  int iterNum = 0;
  double minCost = 0;
  double lastCost = 0;
  double currCost = 0;
//  int unchanged = 0;
  bool loop = true;
  int *counts = new int[m_clusterNum];
  auto **next_means = new double *[m_clusterNum];    // New model for reestimation
  for (int i = 0; i < m_clusterNum; i++) {
    next_means[i] = new double[m_dimNum];
  }

  while (loop) {
    memset(counts, 0, sizeof(int) * m_clusterNum);
    for (int i = 0; i < m_clusterNum; i++) {
      memset(next_means[i], 0, sizeof(double) * m_dimNum);
    }

    lastCost = currCost;
    currCost = 0;

    // Classification
    if (num_threads_ > 0) {
      struct ThreadParams *params;
      for (int i = 0; i < num_threads_; i++) {
        params = new ThreadParams{
            i,
            num_threads_,
            m_dimNum,
            m_clusterNum,
            raw_data,
            size,
            &mutex_,
            &currCost,
            counts,
            next_means,
            m_means,
            &bucket_map_,
            dist_cache_,
            label_cache_
        };
        if (pthread_create(&thread_pool_[i], nullptr, thread_classification_work, params)) {
          logger::critical("Error creating thread: %d", i);
        }
      }
      for (int i = 0; i < num_threads_; i++) {
        pthread_join(thread_pool_[i], nullptr);
      }
    } else {
      auto *x = new double[m_dimNum];    // Sample data
      int label = -1;        // Class index
      for (int i = 0; i < size; i++) {
        for (int j = 0; j < m_dimNum; j++)
          x[j] = raw_data.data_[i * m_dimNum + j];

        currCost += GetLabel(x, &label);
        bucket_map_[raw_data.index_[i]] = label;

        counts[label]++;
        for (int d = 0; d < m_dimNum; d++) {
          next_means[label][d] += x[d];
        }
        logger::debug("classified:%d/%d", i, size);
      }
      delete[] x;
    }
    currCost /= size;

    // Reestimation
    for (int i = 0; i < m_clusterNum; i++) {
      if (counts[i] > 0) {
        for (int d = 0; d < m_dimNum; d++) {
          next_means[i][d] /= counts[i];
        }
        std::memcpy(m_means[i], next_means[i], sizeof(double) * m_dimNum);
      }
    }
    //cache based on previous means now invalid
    for (int i = 0; i < cache_size_; i++) {
      dist_cache_[i] = -1;
      label_cache_[i] = -1;
    }

    logger::info("iteration: %d, current average cost: %.6f, last average cost: %.6f", iterNum, currCost, lastCost);
    if(iterNum==0 || currCost < minCost){
      logger::info("persisting iteration %d ...", iterNum);
      SaveCheckpoint(raw_data.prefix_);
      SaveBuckets(raw_data.prefix_);
      minCost = currCost;
    }

    // Terminal conditions
    iterNum++;
    if (iterNum >= m_maxIterNum || (fabs(lastCost - currCost) < m_endError * lastCost)) {
      loop = false;
    }
  }

  delete[] counts;
  for (int i = 0; i < m_clusterNum; i++) {
    delete[] next_means[i];
  }
  delete[] next_means;
  delete[] dist_cache_;
  delete[] label_cache_;
}

void KMeans::Init(const sRawData &raw_data, int N) {
  int size = N;
  if (m_initMode == InitRandom) {
    int interval = size / m_clusterNum;
    auto *sample = new double[m_dimNum];

    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_int_distribution<int> distr(0, size / m_clusterNum);

    for (int i = 0; i < m_clusterNum; i++) {
      int select = interval * i + distr(generator);
      for (int j = 0; j < m_dimNum; j++)
        sample[j] = raw_data.data_[select * m_dimNum + j];
      std::memcpy(m_means[i], sample, sizeof(double) * m_dimNum);
    }
    delete[] sample;
  } else if (m_initMode == InitUniform) {
    auto *sample = new double[m_dimNum];
    for (int i = 0; i < m_clusterNum; i++) {
      int select = i * (size / m_clusterNum);
      for (int j = 0; j < m_dimNum; j++)
        sample[j] = raw_data.data_[select * m_dimNum + j];
      std::memcpy(m_means[i], sample, sizeof(double) * m_dimNum);
    }
    delete[] sample;
  } else if (m_initMode == InitCheckpoint) {
    LoadCheckpoint(raw_data.prefix_);
  } else if (m_initMode == InitPlusPlus) {
    logger::info("initializing kmeans++...");

    //randomly initialize 0th centroid
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());
    std::uniform_int_distribution<int> distr(0, size);
    int select = distr(generator);

    auto *sample = new double[m_dimNum];
    for (int j = 0; j < m_dimNum; j++) {
      sample[j] = raw_data.data_[select * m_dimNum + j];
    }
    std::memcpy(m_means[0], sample, sizeof(double) * m_dimNum);

    struct PPThreadParams *params;
    auto *dist = new double[size];
    for (int c = 1; c < m_clusterNum; c++) {
      for (int i = 0; i < num_threads_; i++) {
        params = new PPThreadParams{i,
            m_dimNum,
            c,
            m_means,
            raw_data.data_,
            dist,
            size,
            num_threads_,
            &cache_,
            &mutex_};
        if (pthread_create(&thread_pool_[i], nullptr, thread_initplusplus_work, params)) {
          logger::critical("Error creating thread: %d", i);
        }
      }

      for (int i = 0; i < num_threads_; i++) {
        pthread_join(thread_pool_[i], nullptr);
      }
      int max_index = std::distance(dist, std::max_element(dist, dist + size));
//      logger::debug("cache_size=%d, selected %d for cluster %d", cache_.size(), max_index, c);
      std::vector<double> test_dist(dist, dist + size);
      for (int k = 0; k < m_dimNum; k++)
        sample[k] = raw_data.data_[max_index * m_dimNum + k];
      std::memcpy(m_means[c], sample, sizeof(double) * m_dimNum);
      cache_.clear();
    }
    delete[] dist;
  }
}

double KMeans::GetLabel(const double *sample, int *label) {
  double dist = std::numeric_limits<double>::max();
  for (int i = 0; i < m_clusterNum; i++) {
    double temp = CalcDistance(sample, m_means[i], m_dimNum);
    if (temp < dist) {
      dist = temp;
      *label = i;
    }
  }
  return dist;
}

double KMeans::EuclideanDistance(const double *x, const double *u, int dimNum) {
  double temp = 0;
  for (int d = 0; d < dimNum; d++) {
    temp += (x[d] - u[d]) * (x[d] - u[d]);
  }
  return sqrt(temp);
}

double KMeans::EarthMoversDistance(const double *x, const double *u, int dimNum) {
  if (dimNum != 50) {
    logger::error("unsupported dimension %d, unable to find distance", dimNum);
    return 0;
  }
  EMD emd(x, u);
  return emd.Compute(0, 0);
}

double KMeans::CalcDistance(const double *x, const double *u, int dimNum) {
  if (dimNum > 1) {
    return EarthMoversDistance(x, u, dimNum);
  }
  return EuclideanDistance(x, u, dimNum);
}

void KMeans::SaveBuckets(const std::string &prefix) {
  // Output the label file
  fs::path dir(BULLDOG_DIR_DATA_ABS);
  fs::path file(BUCKET_FILE_PREFIX + "_" + prefix + BUCKET_FILE_EXT);
  fs::path full_path = dir / file;
  logger::info("saving bucket of size %d", bucket_map_.size());
  Bucket::Save(bucket_map_, full_path);
  logger::info("bucket saved to %s", full_path);
}

void KMeans::SaveCheckpoint(const std::string &prefix) {
  fs::path dir(BULLDOG_DIR_DATA_ABS);
  fs::path file(BUCKET_CP_PREFIX + "_" + prefix + BUCKET_CP_EXT);
  fs::path full_path = dir / file;

  std::ofstream os(full_path, std::ios::binary | std::ios::trunc);
  if (os.is_open()) {
    os << m_dimNum << std::endl;
    os << m_clusterNum << std::endl;
    for (int i = 0; i < m_clusterNum; i++) {
      for (int d = 0; d < m_dimNum; d++) {
        os << m_means[i][d] << ",";
      }
      os << std::endl;
    }
    os.close();
    logger::info("checkpoint saved to %s", full_path);
  } else {
    logger::error("unable to open file %s", full_path);
    exit(EXIT_FAILURE);
  }
}

void KMeans::LoadCheckpoint(const std::string &prefix) {
  fs::path dir(BULLDOG_DIR_DATA_ABS);
  fs::path file(BUCKET_CP_PREFIX + "_" + prefix + BUCKET_CP_EXT);
  fs::path full_path = dir / file;

  std::ifstream is(full_path, std::ios::binary);
  if (is.is_open()) {
    std::string line, item;
    getline(is, line);
    int dim_num = std::stoi(line);
    getline(is, line);
    int cluster_num = std::stoi(line);

    if (dim_num != m_dimNum) {
      logger::error("dimension from file %d != %d", dim_num, m_dimNum);
      exit(EXIT_FAILURE);
    }
    if (cluster_num != m_clusterNum) {
      logger::error("number of clusters from file %d != %d", dim_num, m_dimNum);
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < m_clusterNum; i++) {
      getline(is, line);
      std::stringstream s(line);
      for (int d = 0; d < m_dimNum; d++) {
        getline(s, item, ',');
        m_means[i][d] = std::stod(item);
      }
    }
  } else {
    logger::error("unable to open file %s", full_path);
    exit(EXIT_FAILURE);
  }
}