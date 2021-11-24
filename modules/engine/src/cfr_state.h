//
// Created by Isaac Zhang on 5/9/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_CFR_STATE_H_
#define BULLDOG_MODULES_ENGINE_SRC_CFR_STATE_H_

/**
 * to determine when the searcher should stop.
 * by time, by max_iter, by residual sigma
 * -1 means not applicable
 *
 * < means keep searching
 */

#include <bulldog/logger.hpp>
#include <shared_mutex>
#include <numeric>

struct sCFRState {
  sCFRState() {}
  sCFRState(double time_milli_seconds, int iteration, double exploitability, double expl_std) : time_milli_seconds(
      time_milli_seconds), iteration(iteration), exploitability(exploitability), expl_std(expl_std) {}

  double time_milli_seconds = -1;
  int iteration = 0;
  double exploitability = -1;
  double expl_std = -1;
  std::vector<double> window_expl;

  virtual ~sCFRState() = default;

  void UpdateState(int iter_increment, double time, double explt) {
    if (std::isnan(time) || std::isnan(explt)) {
      logger::error("====== Update State Values are NaN!!! ======");
    }
    iteration += iter_increment;
    time_milli_seconds = time;
    exploitability = explt;
    addToWindow(explt);
  }

  bool operator<(const sCFRState &that) {
    bool time_flag = that.time_milli_seconds == -1 || this->time_milli_seconds < that.time_milli_seconds;
    bool iter_flag = that.iteration == -1 || this->iteration < that.iteration;
    bool expt_flag = that.exploitability == -1 || this->exploitability > that.exploitability;
    bool std_flag = that.expl_std == -1 || this->expl_std > that.expl_std;
    if (!(time_flag && iter_flag && expt_flag && std_flag)) {
      if (!time_flag) logger::info("cfr terminated || time_flag ended at time = %3.f", this->time_milli_seconds);
      if (!iter_flag) logger::info("cfr terminated || iter_flag ended at iteration = %d", this->iteration);
      if (!expt_flag) logger::info("cfr terminated || expt_flag ended at exploitability = %6.f", this->exploitability);
      if (!std_flag)
        logger::info("cfr terminated || std_flag ended at window exploitability standard deviation = %6.f",
                     this->expl_std);
    }
    return time_flag && iter_flag && expt_flag && std_flag;
  }

  //it is called in a thread safe manner.
  void addToWindow(double expl) {
    std::vector<double> &v = this->window_expl;

    //hard coded window size
    if (v.size() == 10) {
      v.erase(v.begin());
    }
    v.push_back(expl);
    //calculate std
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    double mean = sum / v.size();
    std::vector<double> diff(v.size());
    std::transform(v.begin(), v.end(), diff.begin(), [mean](double x) { return x - mean; });
    double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    this->expl_std = std::sqrt(sq_sum / v.size());
  }

  void Print() {
    logger::info("iter(%d) || expl = %f || expl_std = %f ||total_training_time = %f (ms)",
                 iteration,
                 exploitability,
                 expl_std,
                 time_milli_seconds);
  }
};
#endif //BULLDOG_MODULES_ENGINE_SRC_CFR_STATE_H_
