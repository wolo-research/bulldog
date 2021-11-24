//
// Created by Isaac Zhang on 8/13/20.
//

#ifndef BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_GAME_UTIL_H_
#define BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_GAME_UTIL_H_

extern "C" {
#include "game.h"
}

#include <chrono>

int DelayAction(std::chrono::system_clock::time_point start_time, int milli_available, int max_delay);
bool IsPriorStateSameRound(State* old_state, State* new_state);
bool StateBettingEqual(State* a, State* b);
int32_t GetTotalPot(State& state);//todo: m-player
bool WithEarlyRaiseAction(State* state);
bool IsStateCardsValid(Game *game, State *state);


#endif //BULLDOG_MODULES_CORE_INCLUDE_BULLDOG_GAME_UTIL_H_
