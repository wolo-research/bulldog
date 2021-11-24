//
// Created by Isaac Zhang on 8/13/20.
//
#include "../../core/include/bulldog/game_util.h"
#include <cmath>
#include <set>
#include <vector>

/*
 * only match by action sequence. not complete but will do for our use cases
 * now it is not safe, as we assume it is in the same round.
 */
bool IsPriorStateSameRound(State *old_state, State *new_state) {
  uint8_t canon_round = old_state->round;
  if (canon_round != new_state->round) return false;
  uint8_t a_max = old_state->numActions[canon_round];
  if (a_max >= new_state->numActions[canon_round])
    return false; //cant have the same number of actions
  //else, for each action in the old state, it should be found in the new state
  //if none action of the old_state, it returns true
  for (auto a = 0; a < a_max; a++) {
    int32_t old_code = actionToCode(&old_state->action[canon_round][a]);
    int32_t new_code = actionToCode(&new_state->action[canon_round][a]);
    if (old_code != new_code) return false;
  }
  return true;
}
bool StateBettingEqual(State *a, State *b) {
  /* make sure the betting is the same */
  if (a->round != b->round) {
    return false;
  }

  for (auto r = 0; r <= a->round; ++r) {

    if (a->numActions[r] != b->numActions[r]) {
      return false;
    }

    for (auto i = 0; i < a->numActions[r]; ++i) {

      if (a->action[r][i].type != b->action[r][i].type) {
        return false;
      }
      if (a->action[r][i].size != b->action[r][i].size) {
        return false;
      }
    }
  }
  return true;
}

bool WithEarlyRaiseAction(State *state) {
  //first find out case if any one raise before you at the round
  int r = state->round;
  //for preflop, it is always consider raise (big blind)
  if (r == 0) {
    return true;
  }

  //for > preflop, if someone else raise before,
  if (state->numActions[r] > 0)
    for (int a = 0; a < state->numActions[r]; a++)
      if (state->action[r][a].type == a_raise)
        return true;

  return false;
}

int32_t GetTotalPot(State &state) {
  return state.spent[0] + state.spent[1];
}

//check card crash
bool IsStateCardsValid(Game *game, State *state) {
  std::set<uint8_t> cards_s;
  std::vector<uint8_t> cards_v;

  //add hole cards
  for (auto p = 0; p < game->numPlayers; p++) {
      for (auto c = 0; c < game->numHoleCards; c++) {
          if (state->holeCards[p][c] != UNDEFINED_CARD) {
              cards_s.insert(state->holeCards[p][c]);
              cards_v.push_back(state->holeCards[p][c]);
          }
      }
  }
  
  //then check all the board + hands cards, if they has the same one
  auto sum_bc = sumBoardCards(game, state->round);
  for (auto i = 0; i < sum_bc; i++) {
      if (state->boardCards[i] != UNDEFINED_CARD) {
          cards_s.insert(state->boardCards[i]);
          cards_v.push_back(state->boardCards[i]);
      }
  }

  if (cards_s.size() != cards_v.size())  //should be the same
    return false;
  return true;
}

int DelayAction(std::chrono::system_clock::time_point start_time, int milli_available, int buffer) {
    srand(time(NULL));
    auto duration_milli = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time).count();
    int milli_remaining = milli_available - duration_milli;
    if (milli_remaining > buffer) {
        auto random = ((double)rand() / (RAND_MAX)); // random between 0 and 1
        return static_cast<int>(buffer * 0.5 * random);
    }
    return 0;
}
