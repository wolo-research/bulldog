//
// Created by Isaac Zhang on 5/7/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_CFR_PARAM_H_
#define BULLDOG_MODULES_ENGINE_SRC_CFR_PARAM_H_

#include <string>
#include <map>
#include <cpprest/json.h>
#include "filesystem"
/*
   * CFR Param Enums
  */
enum CFU_COMPUTE_MODE {
  BEST_RESPONSE,
  WEIGHTED_RESPONSE,
  SUM_RESPONSE
};

static std::map<std::string, CFU_COMPUTE_MODE> RollinEstimatorMap{
    {"best_resp", BEST_RESPONSE},
    {"weighted_resp", WEIGHTED_RESPONSE},
    {"sum_resp", SUM_RESPONSE},
};

//avg update currently only supports weighted average configuration
enum AVG_UPDATE {
  AVG_OFF,
  AVG_CLASSIC,
  CFRP
};
static std::map<std::string, AVG_UPDATE> AverageUpdateMap{
    {"classic", AVG_CLASSIC},
    {"off", AVG_OFF},
    {"cfrp", CFRP}
};

enum THREAD_POLICY {
  SEQ,
  PAR,
  PAR_UNSEQ
};
static std::map<std::string, THREAD_POLICY> ThreadPolicyMap{
    {"sequenced", SEQ},
    {"parallel", PAR},
    {"parallel_unsequenced", PAR_UNSEQ}
};

enum CFR_MODE{
  CFR_UNKNOWN,
  CFR_VECTOR_PAIRWISE_SOLVE,
  CFR_VECTOR_ALTERNATE_SOLVE,
  CFR_SCALAR_SOLVE,
};
static std::map<std::string, CFR_MODE> CfrModeMap{
    {"", CFR_UNKNOWN},
    {"vector", CFR_VECTOR_PAIRWISE_SOLVE},
    {"vector_alt", CFR_VECTOR_ALTERNATE_SOLVE},
    {"scalar", CFR_SCALAR_SOLVE},
};

enum STRATEGY_TYPE {
  UNKNOWN,
  STRATEGY_WAVG,
  STRATEGY_REG,
  STRATEGY_ZIPAVG
};
static std::map<std::string, STRATEGY_TYPE> StrategyMap{
    {"", UNKNOWN},
    {"regret", STRATEGY_REG},
    {"wavg", STRATEGY_WAVG},
    {"zipavg", STRATEGY_ZIPAVG}
};
static std::map<STRATEGY_TYPE, std::string> StrategyToNameMap{
        {UNKNOWN, "unknown"},
        {STRATEGY_REG, "regret"},
        {STRATEGY_ZIPAVG, "zipavg"},
        {STRATEGY_WAVG, "wavg"}
};

struct sCfrParam {
  web::json::value raw_;
  unsigned int num_threads = 1;
  std::string name;

  int iteration=0;
  /*
   * rollout
   */
  double rollout_prune_thres = 0.9; //of floor regret
  double rollout_prune_prob = 0.9;
  bool pruning_on = false;
  //dls
  bool depth_limited = false;
  int depth_limited_rollout_reps_ = 3;
  bool depth_limited_cache_ = false;

  /*
   * rollin  util upwards, my and opp
   */
  CFU_COMPUTE_MODE cfu_compute_acting_playing = WEIGHTED_RESPONSE;
  CFU_COMPUTE_MODE cfu_compute_opponent = SUM_RESPONSE;

  /*
   * regret matching
   */
  double rm_floor = -1.0; // number of big blind. default no flooring
  AVG_UPDATE rm_avg_update = AVG_OFF;
  //discounting
  int rm_disc_interval = -1;
  double lcfr_alpha = 1.5;

  STRATEGY_TYPE strategy_cal_mode_ = STRATEGY_REG;
  STRATEGY_TYPE profiling_strategy_ = STRATEGY_WAVG;
  CFR_MODE cfr_mode_ = CFR_VECTOR_PAIRWISE_SOLVE;

  bool regret_learning_on =true;

  //cfrs specific
  bool avg_side_update_ = true;

  void SaveCFRConfig(const std::string &prefix) {
    std::filesystem::path dir(BULLDOG_DIR_DATA_STG);
    std::ofstream file(dir / (prefix + ".cfr"), std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
      file << raw_.serialize() << std::endl;
      file.close();
    } else {
      logger::error("unable to open file %s", dir / (prefix + ".cfr"));
    }
  }
  void ProfileModeOn() {
    //use only vector cfr, pairwise only
    cfr_mode_ = CFR_VECTOR_ALTERNATE_SOLVE;
    //use best response on acting playing
    cfu_compute_acting_playing = BEST_RESPONSE;
    regret_learning_on = false;
    //turn off wavg update in range rollout
    rm_avg_update = AVG_OFF;
    strategy_cal_mode_ = profiling_strategy_;
    pruning_on = false;
  }

};
#endif //BULLDOG_MODULES_ENGINE_SRC_CFR_PARAM_H_
