//
// Created by Isaac Zhang on 3/24/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_ABSTRACT_GAME_H_
#define BULLDOG_MODULES_ENGINE_SRC_ABSTRACT_GAME_H_

#include "rnba_kernel.hpp"
#include "node.h"
#include "bucket_reader.hpp"
#include "hand_belief.h"
#include "hand_kernel.hpp"
#include <cpprest/json.h>

extern "C" {
#include <bulldog/game.h>
};

struct NodeMatchCondition {
  NodeMatchCondition() {}

  NodeMatchCondition(Node *matched_node, double off_tree_dist, int edit_distance, int betting_size_distance)
      : matched_node_(matched_node),
        off_tree_dist_(off_tree_dist),
        bet_sim_dist_(edit_distance),
        betting_size_distance(betting_size_distance) {}

  NodeMatchCondition(State &real_state, Node *node) {
    off_tree_dist_ = PotL2(real_state, node->state_);
    bet_sim_dist_ = SumBettingPatternDiff(&real_state, &node->state_);
    if (bet_sim_dist_ == 0)
      betting_size_distance = DecayingBettingDistance(real_state, node->state_);
    matched_node_ = node;
  }

  Node *matched_node_ = nullptr;
  double off_tree_dist_ = -1;
  int bet_sim_dist_ = -1;
  //this is just for internal comparison with betting size considered. not used normally
  int betting_size_distance = -1;

  void Print(std::string msg = "") const {
    char prefix[1024];
    sprintf(prefix, "%s MATCH NODE CONDITION [L2:%.3f][SIM:%d][ROUND:%d][BETTING_DIST:%d]: ",
            msg.c_str(),
            off_tree_dist_,
            bet_sim_dist_,
            matched_node_->GetRound(),
            betting_size_distance);
    matched_node_->PrintState(std::string(prefix));
    if (off_tree_dist_ > 1000 && matched_node_->GetRound() == HOLDEM_ROUND_PREFLOP)
      logger::warn("off_tree distance [%f] > 1000 | r = %d | sim_dist = %d",
                   off_tree_dist_,
                   matched_node_->GetRound(),
                   bet_sim_dist_);
  }

  void CopyValue(NodeMatchCondition &that) {
    matched_node_ = that.matched_node_;
    off_tree_dist_ = that.off_tree_dist_;
    bet_sim_dist_ = that.bet_sim_dist_;
    betting_size_distance = that.betting_size_distance;
  }

  bool operator<(const NodeMatchCondition &that) const {
    if (bet_sim_dist_ < that.bet_sim_dist_) return true;
    if (bet_sim_dist_ > that.bet_sim_dist_) return false;
    // path sim ==
    if (bet_sim_dist_ == 0) {
      //only if it is 0
      if (betting_size_distance < that.betting_size_distance) return true;
      if (betting_size_distance > that.betting_size_distance) return false;
    }
    // betting with size edit distance ====
    if (off_tree_dist_ < that.off_tree_dist_) return true;
    if (off_tree_dist_ > that.off_tree_dist_) return false;
    // L2 ==, return the one with lower node_id, or u may have two diff match in a same round otherwise
    return matched_node_->GetN() < that.matched_node_->GetN();
  }
};

class AbstractGame {
 public:
  explicit AbstractGame() {
    root_hand_belief_ = new sHandBelief[player_num_];
  };

  virtual ~AbstractGame() {
    delete kernel_;
    if (root_node_ != nullptr)
      Node::DestroyBettingTree(root_node_);
    delete[] root_hand_belief_;
    logger::trace("abstract game object gracefully shutting down");
  }

  std::string name_ = "default_ag";
  Game game_{};
  BucketReader bucket_reader_;
  State root_state_{};
  sRNBAKernel *kernel_ = nullptr;
  Node *root_node_ = nullptr;

  //assume to be a fair estimate of the reach_prob of the root_node.
  sHandBelief *root_hand_belief_ = nullptr;
  int player_num_ = 2;

  web::json::value raw_;
  bool depth_limited_ = false;

  /*
   * query game state methods
   */
  static int GetActivePlayerNum() {
    return 2;
  };

  int32_t GetBigBlind() {
    return BigBlind(&game_);
  };

  void Print() const {
    logger::breaker();
    logger::debug("abstract game <- print()");
    kernel_->Print();
    logger::breaker();
  }

  void IndexBettingTree(Node *this_node = nullptr);

  void MapToNode(State &real_state, NodeMatchCondition &condition);

  void NormalizeRootReachProb();

  void BuildKernelFromRootNode(Bucket_t *bucket_counts);

  void PurifyLowProbRange() const;

  [[nodiscard]] int GetMaxRound() const;

  std::map<uint8_t, std::multimap<uint8_t, Node *>> node_map_;
};

#endif //BULLDOG_MODULES_ENGINE_SRC_ABSTRACT_GAME_H_
