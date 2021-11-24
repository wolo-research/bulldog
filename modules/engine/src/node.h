//
// Created by Isaac Zhang on 3/5/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_NODE_H_
#define BULLDOG_MODULES_ENGINE_SRC_NODE_H_

#include <vector>
#include <bulldog/logger.hpp>
#include <stack>
#include "cfr_param.h"
#include <shared_mutex>
#include <set>

extern "C" {
#include "bulldog/game.h"
};

/**
 * Node is the betting node in the betting tree. but we write it as a graph
 */

using Round_t = uint8_t;
using Node_t = uint32_t;
using Bucket_t = uint32_t;
using Action_t = uint8_t;
const Bucket_t INVALID_BUCKET = 100000000; // we are not using more than 100m

struct NodeValueCache {
  /*
   * load from files by the state string .e.g cr200r600c
   */
  void Load() {};
  bool Exists(Bucket_t b0, Bucket_t b1) {
    mutex.lock_shared();
    auto v = cache_.find(Hash(b0, b1)) != cache_.end();
    mutex.unlock_shared();
    return v;
  }
  double GetValue(Bucket_t b0, Bucket_t b1) {
    mutex.lock_shared();
    double v = cache_[Hash(b0, b1)];
    mutex.unlock_shared();
    return v;
  }
  void SetValue(Bucket_t b0, Bucket_t b1, double v) {
    mutex.lock();
    cache_[Hash(b0, b1)] = v;
    mutex.unlock();
  }
  //normally this is either used in preflop (b <= 169, orsubgame solving e.g. 2 <= 2500)
  inline uint64_t Hash(Bucket_t b0, Bucket_t b1) {
    return b0 * pow(2, 20) + b1;
  }
  //by default the value are from the view point of player 0. todo: m player
  std::unordered_map<uint64_t , double> cache_;
  std::shared_mutex mutex;
};

class Node {
 public:
  Node() = default;

  virtual ~Node();
  Node(Game *game, Node *parent_node);
  void SetState(State state);
  void SetIndex(Node_t index);
  [[nodiscard]] uint8_t GetKthAction() const;
  [[nodiscard]] int GetAmax();
  [[nodiscard]] bool FollowingChanceNode() const;
  int GetLastActionCode();
  [[nodiscard]] bool IsTerminal() const;
  [[nodiscard]] bool IsLeafNode() const;
  Action GetLastAction();
  void PrintState(const std::string &prefix = "");
  void PrintAllChildState();
  int GetDebugCode();
  bool IsShowdown();
  int GetActingPlayer();
  virtual int GetSpent(uint8_t position);

  [[nodiscard]] Round_t GetRound() const;
  [[nodiscard]] Node_t GetN() const;
  int GetStake(int my_pos);
  [[nodiscard]] std::stack<int> GetPathFromRoot() const;
  void SortChildNodes();
  int GetSumPot();
  int GetTotalActionDepth();
  static void DestroyBettingTree(Node *this_node);

  /*
   * strategy related functions
   */
  void InitAdjustmentFactor_r(Bucket_t *buckets_by_round);

  //children must be sorted, by the actionToCode.
  std::vector<Node *> children;
  Node *parent_node_ = nullptr;
  pthread_mutex_t mutex_{};
  int sibling_idx_ = -1; //default for terminal node.
  State state_{};
  Game *game_;
  int a_max = -1;
  Node_t index_{};

  //for vector cfr adjustment, capped at 20 actualy
  uint8_t *reach_adjustment = nullptr;

  //for depth limit subgame
  bool is_leaf_node = false;
  NodeValueCache *value_cache_ = nullptr;
  void CreateValueCacheIfNull();

  //for freezing 1 item only
  Bucket_t frozen_b = INVALID_BUCKET;
 private:
  static void DestroyBettingTreeR(Node *this_node);
};

#endif //BULLDOG_MODULES_ENGINE_SRC_NODE_H_
