//
// Created by wolo on 11/7/2020.
//

#ifndef BULLDOG_CONSTANT_H
#define BULLDOG_CONSTANT_H

#include <cstdint>
#include <algorithm>
extern "C" {
#include "bulldog/game.h"
}

/*
 * a lot of constants
 */
using ULONG_WAVG = uint64_t;
using UINT_WAVG = uint32_t;
using DOUBLE_REGRET = double;
using INT_REGRET = int;
using ZIPAVG = uint8_t;

//strategy purification etc
const double DEFAULT_BAYESIAN_TRANSITION_FILTER = 0.03;
const double RANGE_ROLLOUT_PRUNE_THRESHOLD = 0.01;
//epsilon
const double DOUBLE_EPSILON = 0.00000000001;

/*
 * Strategy Helper class
 */
void CheckAvgSum(float *avg, int size);
bool IsAvgUniform(float *avg, int size);
void NormalizePolicy(float* avg, int size);

template<typename T>
int GetPolicy(float *avg, int size, T *ptr) {
//  bool integral = std::is_integral<T>::value;
  T positive_v[size];
  T sum_pos_v = 0;
  for (int a = 0; a < size; a++) {
    positive_v[a] = std::max<T>(0, ptr[a]);
//    T v = ptr[a];
//    positive_v[a] = v > 0 ? v : 0;;
//      new_pos_reg[a] = regret_[rnba] > 0.0 ? regret_[rnba] : 0.0;
// in multithread setting this may have problem.
    sum_pos_v += positive_v[a];
  }

  if (sum_pos_v > 0) {
    for (int a = 0; a < size; a++) {
      avg[a] = (float) positive_v[a] / sum_pos_v;
      //necessary?
      if (avg[a] < 0)
        return 1;
    }
  } else {
    for (int a = 0; a < size; a++) {
      avg[a] = (float) 1.0 / (float) size;
    }
  }
  return 0;
};
#endif //BULLDOG_CONSTANT_H
