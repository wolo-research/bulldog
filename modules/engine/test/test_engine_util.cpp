//
// Created by Isaac Zhang on 8/7/20.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../src/engine_util.h"

bool FloatEqual(float a, float b) {
  return fabs(a - b) < 0.00001;
}

template <typename T>
bool TestStrategybaseType(){
  T d_[4];
  float d_avg[4];
  d_[0] = 10.0;
  d_[1] = 20.0;
  d_[2] = 30.0;
  d_[3] = 40.0;
  REQUIRE(GetPolicy<T>(d_avg, 4, d_) == 0);
  REQUIRE(FloatEqual(d_avg[0], 0.1));
  REQUIRE(FloatEqual(d_avg[1], 0.2));
  REQUIRE(FloatEqual(d_avg[2], 0.3));
  REQUIRE(FloatEqual(d_avg[3], 0.4));

  T d2_[3];
  d2_[0] = 0;
  d2_[1] = 0;
  d2_[2] = 0;
  REQUIRE(GetPolicy<T>(d_avg, 3, d2_) == 0);
  REQUIRE(FloatEqual(d_avg[0], 0.333333));
  REQUIRE(FloatEqual(d_avg[1], 0.333333));
  REQUIRE(FloatEqual(d_avg[2], 0.333333));
  return true;
}

TEST_CASE("compute strategy template method", "[util]") {
  REQUIRE(TestStrategybaseType<double>());
  REQUIRE(TestStrategybaseType<int>());
  REQUIRE(TestStrategybaseType<uint64_t>());
  REQUIRE(TestStrategybaseType<uint32_t>());
  {
    /*
     * double
     */
    double d_[4];
    float d_avg[4];
    d_[0] = 10.0;
    d_[1] = 20.0;
    d_[2] = 30.0;
    d_[3] = 40.0;
    REQUIRE(GetPolicy<double>(d_avg, 4, d_) == 0);
    REQUIRE(FloatEqual(d_avg[0], 0.1));
    REQUIRE(FloatEqual(d_avg[1], 0.2));
    REQUIRE(FloatEqual(d_avg[2], 0.3));
    REQUIRE(FloatEqual(d_avg[3], 0.4));

    double d2_[3];
    d2_[0] = 0;
    d2_[1] = 0;
    d2_[2] = 0;
    REQUIRE(GetPolicy<double>(d_avg, 3, d2_) == 0);
    REQUIRE(FloatEqual(d_avg[0], 0.333333));
    REQUIRE(FloatEqual(d_avg[1], 0.333333));
    REQUIRE(FloatEqual(d_avg[2], 0.333333));
  }
}