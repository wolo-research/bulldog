//
// Created by Carmen C on 18/5/2020.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_IO_H_
#define BULLDOG_MODULES_ENGINE_SRC_IO_H_

//#include "pstl.hpp"
#include "strategy.h"
/*
 * AG
 */
void SaveAG(Strategy *target, const std::string &prefix);
void LoadAG(Strategy *target, const std::string &prefix, BucketPool *bucket_pool, const char *state_str);

//todo: read REGRET from type, with a template function.
//todo: return pointer as a template function
void SaveStrategy(Strategy *target, STRATEGY_TYPE type, const std::string &prefix);
void LoadStrategy(Strategy *target, STRATEGY_TYPE type, const std::string &prefix, bool disk_lookup = false);

template <class T>
void SaveAssertMap(Strategy *target, STRATEGY_TYPE type, const std::string &prefix);

template <class T>
void LoadAssertMap(Strategy *target, STRATEGY_TYPE type, const std::string &prefix);
#endif //BULLDOG_MODULES_ENGINE_SRC_IO_H_
