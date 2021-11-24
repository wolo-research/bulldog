//
// Created by Isaac Zhang on 6/5/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_CFR_COMMAND_HPP_
#define BULLDOG_MODULES_ENGINE_SRC_CFR_COMMAND_HPP_

#include <map>
#include <bulldog/logger.hpp>

enum CFR_COMMAND {
    CMD_SCALAR_SOLVE,
    CMD_VECTOR_PAIRWISE_SOLVE,
    CMD_VECTOR_ALTERNATE_SOLVE,
    CMD_DISCOUNTING,
    CMD_SAVE_REG_WAVG,
    CMD_VECTOR_PROFILING,
    CMD_AVG_UPDATE_ON,
    CMD_VECTOR_PRUNING_ON,
    CMD_PRINT_ROOT_STG,
    CMD_DUMMY
};

static std::map<CFR_COMMAND, std::string> CfrCommandName{
        {CMD_DISCOUNTING,           "CMD_DISCOUNTING"},
        {CMD_VECTOR_PAIRWISE_SOLVE, "CMD_VECTOR_PAIRWISE_SOLVE"},
        {CMD_VECTOR_ALTERNATE_SOLVE, "CMD_VECTOR_ALTERNATE_SOLVE"},
        {CMD_SCALAR_SOLVE,          "CMD_SCALAR_SOLVE"},
        {CMD_SAVE_REG_WAVG,         "CMD_SAVE_REG_WAVG"},
        {CMD_VECTOR_PROFILING,      "CMD_VECTOR_PROFILING"},
        {CMD_VECTOR_PRUNING_ON,     "CMD_VECTOR_PRUNING_ON"},
        {CMD_AVG_UPDATE_ON,         "CMD_AVG_UPDATE_ON"},
        {CMD_PRINT_ROOT_STG,        "CMD_PRINT_ROOT_STG"},
};

struct CfrCommandWrapper {
    int trigger_iter_ = -1;
    CFR_COMMAND type_;
    int steps_ = 0;

    CfrCommandWrapper(int trigger_iter, CFR_COMMAND type, int steps)
            : trigger_iter_(trigger_iter), type_(type), steps_(steps) {}

    CfrCommandWrapper(int iter, CFR_COMMAND type) : trigger_iter_(iter), type_(type) {};

    inline bool operator<(const CfrCommandWrapper &that) const {
        if (trigger_iter_ < that.trigger_iter_) return true;
        if (trigger_iter_ > that.trigger_iter_) return false;
        return type_ < that.type_;
    }

    void Print() const {
        logger::debug("triggering iter = %d || steps = %d || command = %s", trigger_iter_, steps_,
                      CfrCommandName[type_]);
    }

    void Log(int current_iter, int cmd_time, int total_time) const {
        logger::info("current_iter = %d || command = %s || cmd time = %d (ms) || total time = %d (ms)",
                     current_iter,
                     CfrCommandName[type_],
                     cmd_time,
                     total_time);
    }
};

#endif //BULLDOG_MODULES_ENGINE_SRC_CFR_COMMAND_HPP_
