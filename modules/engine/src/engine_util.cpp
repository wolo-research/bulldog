//
// Created by Isaac Zhang on 8/8/20.
//

#include <bulldog/logger.hpp>
#include "engine_util.h"

void CheckAvgSum(float *avg, int size) {
  float sum = 0.0;
  for (int a = 0; a < size; a++)
    sum += avg[a];
  if (std::fabs(sum - 1.0) > 0.01)
    logger::critical("avg [%f] != 1 dude", sum);
}

//return true if all strategy are the same. i.e. uninitialized.
bool IsAvgUniform(float *avg, int size) {
  if (size == 1) {
    logger::warn("avg with size 1. consider enlarging the abstraction");
    return true; //does not matter
  }
  float uniform_v = (float) 1.0 / (float) size;
  for (int i = 0; i < size; i++)
    //threshold 0.1%
    if (std::fabs(avg[i] - uniform_v) > 0.001)
      return false;

  return true;
}

void NormalizePolicy(float *avg, int size) {
  float sum_local_avg = 0.0;
  for (auto a = 0; a < size; a++)
    sum_local_avg += avg[a];
  auto scaler = 1.0 / sum_local_avg;
  for (auto a = 0; a < size; a++)
    avg[a] *= scaler;
}

