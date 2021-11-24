//
// Created by Carmen C on 4/4/2020.
//

#ifndef BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_CORE_UTIL_HPP_
#define BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_CORE_UTIL_HPP_

#include <string>
#include <bulldog/logger.hpp>

extern "C" {
#include "game.h"
};

struct SimpleTimer {
  std::vector<std::chrono::steady_clock::time_point> ckp_;
  std::vector<std::string> names_;

  SimpleTimer() {
    ckp_.push_back(std::chrono::steady_clock::now());
    names_.emplace_back("begin");
  }

  void Checkpoint(const std::string &ckp_name) {
    auto now_ = std::chrono::steady_clock::now();
    logger::debug("TIME SPENT [%s takes %d ms]",
                  ckp_name,
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      now_ - ckp_.at(0)).count());
    ckp_.push_back(now_);
    names_.emplace_back(ckp_name);
  }

  int GetLapseFromBegin() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - ckp_.at(0)).count();
  }

//  void PrintAllCheckpoint() {
//    for (unsigned int i = 1; i < ckp_.size(), i++;) {
//      logger::debug("TIME SPENT [%s takes %d ms]",
//                    names_.at(i),
//                    std::chrono::duration_cast<std::chrono::milliseconds>(
//                        ckp_.at(i) - ckp_.at(0)).count());
//    }
//  }
};

static inline int split_string(std::string s, const std::string delim, std::vector<std::string> &out) {
  size_t pos;
  std::string token;
  int count = 0;
  while ((pos = s.find(delim)) != std::string::npos) {
    token = s.substr(0, pos);
    out.push_back(token);
    s.erase(0, pos + delim.length());
    count++;
  }
  if (!out.empty() && s != "") {
    out.push_back(s);
    count++;
  }
  return count;
}

/**
 *
 * @param s1 real betting
 * @param s2 node mapping
 * @return
 */
static inline int uiLevenshteinDistance(const std::string &s1, const std::string &s2) {
  const int m(s1.size());
  const int n(s2.size());

  if (m == 0) return n;
  if (n == 0) return m;

  auto *costs = new int[n + 1];

  for (int k = 0; k <= n; k++) costs[k] = k;

  int i = 0;
  for (std::string::const_iterator it1 = s1.begin(); it1 != s1.end(); ++it1, ++i) {
    costs[0] = i + 1;
    int corner = i;

    int j = 0;
    for (std::string::const_iterator it2 = s2.begin(); it2 != s2.end(); ++it2, ++j) {
      int upper = costs[j + 1];
      if (*it1 == *it2) {
        costs[j + 1] = corner;
      } else {
        int t(upper < corner ? upper : corner);
        costs[j + 1] = (costs[j] < t ? costs[j] : t) + 1;
      }

      corner = upper;
    }
  }

  int result = costs[n];
  delete[] costs;

  return result;
}

static inline std::string GetBettingStr(std::string rtn) {
  //first remove all digits
  rtn.erase(std::remove_if(rtn.begin(), rtn.end(), [](char c) { return isdigit(c); }),
            rtn.end());
  //then change all c to r
  std::replace_if(rtn.begin(), rtn.end(), [](char c) { return c == 'c'; }, 'r');
  return rtn;
}

static inline std::string GetBettingStr(Game *game, State &state) {
  char line[1024];
  printBetting(game, &state, 1024, line);
  return GetBettingStr(std::string(line));
}

static inline double PotL2(State &ref_state, State &target_state) {
  auto diff_0 = ref_state.spent[0] - target_state.spent[0];
  auto diff_1 = ref_state.spent[1] - target_state.spent[1];
  return sqrt(diff_0 * diff_0 + diff_1 * diff_1);
}

/*
 * return sum of all action size diff upto the latest action of ref state
 * respect the history, where the history nodes have more weights.
 *
 * only use when the edit distance is the same.
 *
 */
static inline int DecayingBettingDistance(State &ref_state, State &target_state) {
  /*
   * added a weight to nodes of the older rounds. half the scale if exists a raise node in the new round.
   * e.g. r200r600c/r900 = real state
   * state 1: r200r400c/r800, sum = 200 * 32 + 100 *16 = 8000
   * state 2: r200r600c/r1200 sum = 0 * 32 + 300 * 16 = 4800, the latter is closer.
   *
   * it also includes some notion of L2 for the same round
   * state 3: r200r600c/r700 sum = 0 * 32 + 200 * 16 = 3200, c is closer than a.
   */
  int s0[4] = {8, 6, 4, 2};
  int sum = 0;
  for (auto r = 0; r <= ref_state.round; r++)
    for (auto a = 0; a < ref_state.numActions[r]; a++) {
      auto ref_type = ref_state.action[r][a].type;
      auto target_type = target_state.action[r][a].type;
      if (ref_type == a_call && target_type == a_call)
        continue;
      if (ref_type == a_raise && target_type == a_raise) {
        int diff_raise_size = abs(ref_state.action[r][a].size - target_state.action[r][a].size);
        sum += diff_raise_size * s0[r];
        continue;
      }
      //for 2p we dont have fold action yet.
      // one call one raise. the diff is the one with the raise action .
      if (ref_type == a_raise) {
        sum += ref_state.action[r][a].size * s0[r];
      } else {
        sum += target_state.action[r][a].size * s0[r];
      }
    }
  return sum;
}

#endif //BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_CORE_UTIL_HPP_