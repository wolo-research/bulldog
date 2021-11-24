//
// Created by Isaac Zhang on 3/5/20.
//

#include <stack>
#include <bulldog/card_util.h>
#include "node.h"
#include "cfr_param.h"

//include both showdown and folding
int Node::GetStake(int my_pos) {
  if (!IsTerminal())
    logger::critical("trying to Get folding value from non folding node");
  int pot = PotStake(&state_);
  if (IsShowdown())
    return pot;
  //folding
  if (state_.playerFolded[my_pos] == 1)
    return -pot;     //I lost
  return pot;  //I win
}
std::stack<int> Node::GetPathFromRoot() const {
  std::stack<int> path;
  if (parent_node_ == nullptr)
    return path;
  path.push(sibling_idx_);
  Node *last_node = parent_node_;
  while (last_node->parent_node_ != nullptr) {
    path.push(last_node->sibling_idx_);
    last_node = last_node->parent_node_;
  }
  return path;
}
void Node::SortChildNodes() {
  //sort the actions so they are in order in the strategy index
  std::sort(children.begin(), children.end(), [](Node *a, Node *b) {
    return a->GetLastActionCode() < b->GetLastActionCode();
  });

  //set their sibling idx
  for (auto a = 0; a < (int) children.size(); a++)
    children[a]->sibling_idx_ = a;
}
int Node::GetSumPot() {
  return GetSpent(0) + GetSpent(1);
}
int Node::GetTotalActionDepth() {
  int depth = 0;
  for (auto r = 0; r <= GetRound(); r++)
    depth += state_.numActions[r];
  return depth;
}
Node::~Node() {
  pthread_mutex_destroy(&mutex_);
  delete[] reach_adjustment;
  delete value_cache_;
}
void Node::InitAdjustmentFactor_r(Bucket_t *buckets_by_round) {
  if (IsTerminal())
    return;
  auto r = GetRound();
  auto local_b_max = buckets_by_round[r];
  //init reach adjustment vector of the node
  if (reach_adjustment == nullptr) {
    reach_adjustment = new uint8_t[local_b_max];
    for (int b = 0; b < (int) local_b_max; b++)
      reach_adjustment[b] = 4;
  }
  for (auto c: children)
    c->InitAdjustmentFactor_r(buckets_by_round);
}

/*
 * strategy
 */
int Node::GetDebugCode() {
  long base = 0;
  auto action_count = state_.numActions[state_.round];
  auto p = action_count;
  //the first indicate who acts first
  auto first_act = state_.actingPlayer[state_.round][0];
  base += first_act * pow(10, p);
  p--;
  for (int i = 0; i < action_count; i++) {
    base += state_.action[state_.round][i].type * pow(10, p);
    p--;
  }
  return base;
}
bool Node::IsShowdown() {
  if (!IsTerminal()) return false;
  //finished, not sure it is a call or fold
  if (state_.playerFolded[0] == 1) return false;
  return state_.playerFolded[1] != 1;
}
int Node::GetActingPlayer() {
  if (IsTerminal())
    logger::critical("terminal node has no acting player.");
  auto player = currentPlayer(game_, &state_);
  if (!(player == 0 || player == 1))
    logger::critical("get player error %d", player);
  return currentPlayer(game_, &state_);
}
int Node::GetSpent(uint8_t position) {
  if (position >= 2)
    logger::critical("yet to support multiple players");
  return state_.spent[position];
}
Action Node::GetLastAction() {
  if (state_.round == 0 && state_.numActions[0] == 0)
    logger::critical("no last action for empty game root node");
  return GetLastActionFromState(&state_);
}
bool Node::IsTerminal() const {
  return state_.finished == 1;
}
int Node::GetLastActionCode() {
  auto action = GetLastAction();
  return actionToCode(&action);
}
bool Node::FollowingChanceNode() const {
  return GetRound() > 0 && GetKthAction() == 0;
}
bool Node::IsLeafNode() const {
  return is_leaf_node;
}
void Node::DestroyBettingTreeR(Node *this_node) {
  if (this_node->IsTerminal())
    return;
  for (auto child_node : this_node->children) {
    DestroyBettingTreeR(child_node);
    delete child_node;
  }
}
Node_t Node::GetN() const {
  return index_;
}
Round_t Node::GetRound() const {
  return state_.round;
}
void Node::PrintAllChildState() {
  for (auto c : children)
    c->PrintState();
}
void Node::PrintState(const std::string &prefix) {
  char line[MAX_LINE_LEN];
  printState(game_, &state_, MAX_LINE_LEN, line);
  logger::debug(prefix + line);
}
int Node::GetAmax() {
  if (a_max == -1)
    a_max = children.size();
  return a_max;
}
uint8_t Node::GetKthAction() const {
  return state_.numActions[state_.round];
}
void Node::SetIndex(Node_t index) {
  index_ = index;
}
void Node::SetState(State state) {
  state_ = state;
}
void Node::DestroyBettingTree(Node *this_node) {
  if (this_node->parent_node_ != nullptr) {
    logger::critical("this is not a root node, cannot destroy betting tree");
  }
  DestroyBettingTreeR(this_node);
  delete this_node;
}
Node::Node(Game *game, Node *parent_node)
    : parent_node_(parent_node),
      game_(game) {
  pthread_mutex_init(&mutex_, nullptr);
}

void Node::CreateValueCacheIfNull() {
  if (value_cache_ == nullptr){
    pthread_mutex_lock(&mutex_);
    //need to check again in case the other thread create it
    if (value_cache_ == nullptr){
      value_cache_ = new NodeValueCache;
      value_cache_->Load();
    }
    pthread_mutex_unlock(&mutex_);
  }
}
