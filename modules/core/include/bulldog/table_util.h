//
// Created by Isaac Zhang on 8/18/20.
//

#ifndef BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_TABLE_UTIL_H_
#define BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_TABLE_UTIL_H_

extern "C" {
#include "game.h"
}
#include "game_util.h"
#include <cmath>
#include <map>
#include <vector>
#include <string>

//for upoker like net_or_raise mode
//if update this order, remember to update all .ohf files
enum TableAction{
  TABLE_ACTION_DEFAULT = -1,
  TABLE_ACTION_BETSIZE,
  TABLE_ACTION_FOLD,
  TABLE_ACTION_CHECK,
  TABLE_ACTION_CALL,
  TABLE_ACTION_MIN_RAISE, //no actual button on screen, only used to determine if betpot screen will show
  TABLE_ACTION_HALF_POT_RAISE,
  TABLE_ACTION_TWO_THIRD_POT_RAISE,
  TABLE_ACTION_ONE_POT_RAISE,
  TABLE_ACTION_2X_RAISE,
  TABLE_ACTION_3X_RAISE,
  TABLE_ACTION_4X_RAISE,
  TABLE_ACTION_ALLIN_RAISE,
  TABLE_ACTION_DOUBLE_POT, //used for allin on betpot screen
};

static std::map<TableAction, std::string> TableActionStringMap{
    {TABLE_ACTION_DEFAULT,"TABLE_ACTION_DEFAULT"},
    {TABLE_ACTION_BETSIZE,"TABLE_ACTION_BETSIZE"},
    {TABLE_ACTION_FOLD,"TABLE_ACTION_FOLD"},
    {TABLE_ACTION_CHECK,"TABLE_ACTION_CHECK"},
    {TABLE_ACTION_CALL,"TABLE_ACTION_CALL"},
    {TABLE_ACTION_MIN_RAISE,"TABLE_ACTION_MIN_RAISE"},
    {TABLE_ACTION_HALF_POT_RAISE,"TABLE_ACTION_HALF_POT_RAISE"},
    {TABLE_ACTION_TWO_THIRD_POT_RAISE,"TABLE_ACTION_TWO_THIRD_POT_RAISE"},
    {TABLE_ACTION_ONE_POT_RAISE,"TABLE_ACTION_ONE_POT_RAISE"},
    {TABLE_ACTION_2X_RAISE,"TABLE_ACTION_2X_RAISE"},
    {TABLE_ACTION_3X_RAISE,"TABLE_ACTION_3X_RAISE"},
    {TABLE_ACTION_4X_RAISE,"TABLE_ACTION_4X_RAISE"},
    {TABLE_ACTION_ALLIN_RAISE,"TABLE_ACTION_ALLIN_RAISE"},
    {TABLE_ACTION_DOUBLE_POT,"TABLE_ACTION_DOUBLE_POT"}
};

static std::map<TableAction, double> TableActionRaiseMultiplier {
    {TABLE_ACTION_2X_RAISE, 2.0},
    {TABLE_ACTION_3X_RAISE, 3.0},
    {TABLE_ACTION_4X_RAISE, 4.0},
    {TABLE_ACTION_HALF_POT_RAISE, 0.5},
    {TABLE_ACTION_TWO_THIRD_POT_RAISE, 0.6667},
    {TABLE_ACTION_ONE_POT_RAISE, 1.0},
};

static std::vector<TableAction> TableActionXRaiseCandidate {
    TABLE_ACTION_HALF_POT_RAISE,
    TABLE_ACTION_TWO_THIRD_POT_RAISE,
    TABLE_ACTION_ONE_POT_RAISE,
    TABLE_ACTION_2X_RAISE,
    TABLE_ACTION_3X_RAISE,
    TABLE_ACTION_4X_RAISE,
};

static std::vector<TableAction> TableActionXRaiseCandidateWithPrior {
    TABLE_ACTION_2X_RAISE,
    TABLE_ACTION_3X_RAISE,
    TABLE_ACTION_4X_RAISE,
};

static std::vector<TableAction> TableActionXRaiseCandidateWithoutPrior {
    TABLE_ACTION_HALF_POT_RAISE,
    TABLE_ACTION_TWO_THIRD_POT_RAISE,
    TABLE_ACTION_ONE_POT_RAISE,
};

bool DoubleCloseEnough(double a, double b);

TableAction ActionToTableAction(Game *game, State &state, Action &action);
bool TableActionToAction(Game* game, State& state, TableAction& table_action, Action& action);
//normally used in handling betsize raise. it will occur only when sgs is not on
TableAction MapToTableAction(Game* game, State& staet, Action &action);
bool IsTableActionPresent(Game *game, State &state, TableAction table_action);

#endif //BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_TABLE_UTIL_H_
