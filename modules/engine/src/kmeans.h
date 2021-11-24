//
// Created by Carmen C on 23/3/2020.
// Modified from https://github.com/luxiaoxun/KMeans-GMM-HMM
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_KMEANS_H_
#define BULLDOG_MODULES_ENGINE_SRC_KMEANS_H_

#include "bucket.h"
extern "C" {
#include "emd.h"
};

class KMeans {
 public:
  enum InitMode {
    InitRandom,
    InitCheckpoint,
    InitUniform,
    InitManual,
    InitPlusPlus,
  };

  struct sRawData{
    double *data_;
    int *cache_key_;
    unsigned int *index_;
    std::string prefix_;
  };

  KMeans(int dimNum, int clusterNum, int num_threads);
  ~KMeans();

  void SetInitMode(int i) { m_initMode = i; }
  void SetMaxIterNum(int i) { m_maxIterNum = i; }
  void SetEndError(double f) { m_endError = f; }
  void SetCacheSize(int i) { cache_size_ = i; }

  double** GetMeans(){ return m_means; }

  void Init(const sRawData& raw_data, int N);
  void Cluster(const sRawData& raw_data, int N);
  void SaveBuckets(const std::string& prefix);

  static double CalcDistance(const double *x, const double *u, int dimNum);
 private:
  int m_dimNum;
  int m_clusterNum;
  double **m_means;

  int m_initMode;
  int m_maxIterNum;        // The stopping criterion regarding the number of iterations
  double m_endError;        // The stopping criterion regarding the error

  int num_threads_;
  pthread_t *thread_pool_;
  pthread_mutex_t mutex_;

  std::map<unsigned int, unsigned short> bucket_map_;
  std::map<std::vector<double>, std::array<double, 2>> cache_;
  double *dist_cache_;
  int *label_cache_;
  int cache_size_;

  double GetLabel(const double *x, int *label);
  static double EuclideanDistance(const double *x, const double *u, int dimNum);
  static double EarthMoversDistance(const double *x, const double *u, int dimNum);

  void SaveCheckpoint(const std::string& prefix);
  void LoadCheckpoint(const std::string& prefix);
};

#endif //BULLDOG_MODULES_ENGINE_SRC_KMEANS_H_
