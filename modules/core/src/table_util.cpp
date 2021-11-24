//
// Created by Isaac Zhang on 8/18/20.
//

#include "../../core/include/bulldog/table_util.h"
#include <algorithm>

TableAction  ActionToTableAction(Game *game, State &state, Action &action) {
  auto acting_player = currentPlayer(game, &state);
  if (action.type == a_fold) return TABLE_ACTION_FOLD;
  if (action.type == a_call) {
    if (state.maxSpent >= state.stackPlayer[acting_player]) {
      //i all-in to opp raise
      return TABLE_ACTION_ALLIN_RAISE;
    }else if(state.maxSpent > state.spent[acting_player]) {
      //i simply call, i still have money
      return TABLE_ACTION_CALL;
    }
    return TABLE_ACTION_CHECK;
  }
  /*
   * handle raise action
   */
  if (action.size == state.stackPlayer[acting_player]) {
    return TABLE_ACTION_ALLIN_RAISE;
  }
  int r = state.round;
  int32_t already_spent = r == 0 ? 0 : state.sum_round_spent[r - 1][acting_player];
  auto raise_at_round = action.size - already_spent;
  if (raise_at_round < 0) {
    printf("impossible to have raise with negative value");
    exit(1);
  }
  auto with_prior = WithEarlyRaiseAction(&state);
  auto raise_mode = with_prior ? X_LAST_RAISE : POT_NET;
  auto raise_base = GetRaiseBase(game, &state, raise_mode);
  double multiplier = (double) raise_at_round / (double) raise_base;
  //find exact match
  if (with_prior) {
    for (auto ta : TableActionXRaiseCandidateWithPrior)
      if (DoubleCloseEnough(multiplier, TableActionRaiseMultiplier[ta]))
        return ta;
  } else {
    for (auto ta : TableActionXRaiseCandidateWithoutPrior)
      if (DoubleCloseEnough(multiplier, TableActionRaiseMultiplier[ta]))
        return ta;
  }
  return TABLE_ACTION_BETSIZE;
}

bool TableActionToAction(Game *game, State &state, TableAction &table_action, Action &action) {
  auto acting_player = currentPlayer(game, &state);
  action.size = 0;
  switch (table_action) {
    case TABLE_ACTION_FOLD:
        action.type = a_fold;
      break;
    case TABLE_ACTION_CHECK: {
        if (state.spent[acting_player] < state.maxSpent)
            return false;
        action.type = a_call;
        break;
    }
    case TABLE_ACTION_CALL: {
        //nth to call
        if (state.spent[acting_player] == state.maxSpent)
            return false;
        //we can only all-in
        if (state.maxSpent >= state.stackPlayer[acting_player])
            return false;
        action.type = a_call;
        break;
    }
    case TABLE_ACTION_HALF_POT_RAISE:
    case TABLE_ACTION_TWO_THIRD_POT_RAISE:
    case TABLE_ACTION_ONE_POT_RAISE:
    case TABLE_ACTION_2X_RAISE:
    case TABLE_ACTION_3X_RAISE:
    case TABLE_ACTION_4X_RAISE: {
      auto raise_mode = WithEarlyRaiseAction(&state) ? X_LAST_RAISE : POT_NET;
      action.type = a_raise;
      int32_t already_spent = state.round == 0 ? 0 : state.sum_round_spent[state.round - 1][acting_player];
      action.size = GetRaiseBase(game, &state, raise_mode) * TableActionRaiseMultiplier[table_action] + already_spent;
      break;
    }
    case TABLE_ACTION_ALLIN_RAISE: {
      if (state.maxSpent >= state.stackPlayer[acting_player]) {
        action.type = a_call;
      } else {
        action.type = a_raise;
        action.size = state.stackPlayer[acting_player];
      }
      break;
    }
    default:
      //todo, handle TABLE_ACTION_MIN_RAISE and TABLE_ACTION_BETSIZE
      return false;
  }
  return true;
}

bool DoubleCloseEnough(double a, double b) {
  return std::fabs(a - b) < 0.01;
}

bool IsTableActionPresent(Game *game, State &state, TableAction table_action) {
  Action action;
  auto acting_player = currentPlayer(game, &state);
  bool success = TableActionToAction(game, state, table_action, action);
  if (success) {
    bool valid = isValidAction(game, &state, 0, &action);
    if (action.type == a_raise) {
        if (table_action != TABLE_ACTION_ALLIN_RAISE && action.size == state.stackPlayer[acting_player]) {
            return false;
        }
        if (table_action == TABLE_ACTION_2X_RAISE
            || table_action == TABLE_ACTION_3X_RAISE
            || table_action == TABLE_ACTION_4X_RAISE) {
            return valid && WithEarlyRaiseAction(&state);
        }
        return valid;
    }
    return valid;
  } else if (table_action == TABLE_ACTION_BETSIZE) {
    return true;
  } else if (table_action == TABLE_ACTION_MIN_RAISE) {
    int min, max;
    //if min raise is all-in, then min will equal to stack
    return raiseIsValid(game, &state, &min, &max) && (min < (state.stackPlayer[acting_player]));
  }
  return false;
}

TableAction MapToTableAction(Game *game, State &state, Action &action) {
  if (action.type != a_raise) {
    printf("you should only try to map raise actions. force return call");
    return TABLE_ACTION_CALL;
  }

  auto with_prior = WithEarlyRaiseAction(&state);
  auto raise_mode = with_prior ? X_LAST_RAISE : POT_NET;
  int r = state.round;
  auto raise_base = GetRaiseBase(game, &state, raise_mode);
  auto acting_player = currentPlayer(game, &state);
  int32_t already_spent = r == 0 ? 0 : state.sum_round_spent[r - 1][acting_player];
  std::vector<std::pair<int, enum TableAction>> candidates;
  if (with_prior) {
    //check 2x, 3x, 4x
    for (auto ta : TableActionXRaiseCandidateWithPrior) {
      if (IsTableActionPresent(game, state, ta)) {
        int size = already_spent + (int) (raise_base * TableActionRaiseMultiplier[ta]);
        candidates.emplace_back(abs(size - action.size), ta);
      }
    }
  } else {
    //check 0.5x, 2/3x, x
    for (auto ta : TableActionXRaiseCandidateWithoutPrior) {
      if (IsTableActionPresent(game, state, ta)) {
        int size = already_spent + (int) (raise_base * TableActionRaiseMultiplier[ta]);
        candidates.emplace_back(abs(size - action.size), ta);
      }
    }
  }
  //also the call action
  candidates.emplace_back(abs(state.spent[1 - acting_player] - action.size), TABLE_ACTION_CALL);
  auto it = std::min_element(candidates.begin(),
                             candidates.end(),
                             [](std::pair<int, TableAction> lhs, std::pair<int, TableAction> rhs) -> bool {
                               if (lhs.first < rhs.first) return true;
                               if (lhs.first > rhs.first) return false;
                               return lhs.second < rhs.second;
                             });
  return it->second;
}