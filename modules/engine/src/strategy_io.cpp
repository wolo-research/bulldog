//
// Created by Carmen C on 11/7/2020.
//

#include "strategy_io.h"
#include "ag_builder.hpp"

#include <filesystem>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <iostream>

void SaveAG(Strategy *target, const std::string &prefix) {
  std::filesystem::path dir(BULLDOG_DIR_DATA_STG);
  {
    auto ag_file = dir / (prefix + ".ag");
    std::ofstream os_ag(ag_file, std::ios::binary | std::ios::trunc);
    if (os_ag.is_open()) {
      os_ag << target->ag_->raw_.serialize() << std::endl;
      os_ag.close();
    } else {
      auto e = std::system_error(errno, std::system_category());
      logger::critical("failed to open %s: %s (system:%d)", ag_file, e.what(), e.code());
    }
  }
  /*
   * save kernel for comparison
   */
  auto kernel = target->ag_->kernel_;
  std::map<std::string, RNBA> assert_map;
  assert_map["max_index"] = kernel->MaxIndex();
  assert_map["nmax_by_r_0"] = kernel->nmax_by_r_[0];
  assert_map["nmax_by_r_1"] = kernel->nmax_by_r_[1];
  assert_map["nmax_by_r_2"] = kernel->nmax_by_r_[2];
  assert_map["nmax_by_r_3"] = kernel->nmax_by_r_[3];
  auto check_file = dir / (prefix + ".kernel");
  std::ofstream os(check_file, std::ios::binary | std::ios::trunc);
  if (os.is_open()) {
    cereal::JSONOutputArchive archive(os);
    try {
      archive(assert_map);
    } catch (cereal::Exception &e) {
      logger::critical("cereal exit with exception:\n %s", e.what());
    }
  } else {
    auto e = std::system_error(errno, std::system_category());
    logger::critical("failed to open %s: %s (system:%d)", check_file, e.what(), e.code());
  }
  os.close();
  logger::debug("save kernel check file for %s", prefix);
}

void LoadAG(Strategy *target, const std::string &prefix, BucketPool *bucket_pool, const char *state_str) {
  std::filesystem::path dir(BULLDOG_DIR_DATA_STG);
  {
    auto ag = new AbstractGame();
    AGBuilder ag_builder((dir / (prefix + ".ag")), bucket_pool);

    if (state_str != nullptr) {
      MatchState state;
      readMatchState(state_str, ag_builder.game_, &state);
      ag_builder.Build(ag, &state.state);
      ag->NormalizeRootReachProb();
    } else {
      ag_builder.Build(ag);
    }
    target->SetAg(ag);
  }
  /*
   * check kernel dimensions
   */
  std::map<std::string, RNBA> assert_map;
  std::ifstream is(dir / (prefix + ".kernel"), std::ios::binary);
  if (is.is_open()) {
    cereal::JSONInputArchive archive(is);
    archive(assert_map);
    is.close();
    auto kernel = target->ag_->kernel_;
    if (assert_map["max_index"] != kernel->MaxIndex())
      logger::critical("max_index diff in kernel check file. file = %d while rebuilt ag = %d",
                       assert_map["max_index"],
                       kernel->MaxIndex());
    std::string base = "nmax_by_r_";
    for (int i = 0; i < 4; i++) {
      auto new_name = base + std::to_string(i);
      if (assert_map[new_name] != kernel->nmax_by_r_[i])
        logger::critical("%s diff in kernel check file. file = %d while rebuilt ag = %d",
                         new_name,
                         assert_map[base + std::to_string(i)],
                         kernel->nmax_by_r_[i]);
    }
    logger::debug("load AG success");
  } else {
    logger::error("unable to open %s", dir / (prefix + ".check_"));
  }
}

void SaveStrategy(Strategy *target, STRATEGY_TYPE type, const std::string &prefix) {
  std::filesystem::path dir(BULLDOG_DIR_DATA_STG);
  auto begin = std::chrono::steady_clock::now();
  std::string full_name = prefix + "." + StrategyToNameMap[type];
  {
    std::ofstream os_reg(dir / full_name, std::ios::binary | std::ios::trunc);
    cereal::BinaryOutputArchive archive(os_reg);
    RNBA size = target->ag_->kernel_->MaxIndex();
    try {
      switch (type) {
        case STRATEGY_REG: {
          if (target->double_regret_ != nullptr) {
            archive(cereal::binary_data(target->double_regret_, sizeof(DOUBLE_REGRET) * size));
          } else {
            archive(cereal::binary_data(target->int_regret_, sizeof(INT_REGRET) * size));
          }
          break;
        }
        case STRATEGY_WAVG: {
          if (target->ulong_wavg_ != nullptr) {
            archive(cereal::binary_data(target->ulong_wavg_, sizeof(ULONG_WAVG) * size));
          } else {
            archive(cereal::binary_data(target->uint_wavg_, sizeof(UINT_WAVG) * size));
          }
          break;
        }
        case STRATEGY_ZIPAVG:archive(cereal::binary_data(target->zipavg_, sizeof(ZIPAVG) * size));
          break;
        default:
          logger::critical("unsupported strategy type %s", StrategyToNameMap[type]);
      }
    } catch (cereal::Exception &e) {
      logger::critical("cereal exit with exception:\n %s", e.what());
    }
    auto end = std::chrono::steady_clock::now();
    auto lapse_milli_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    logger::info("%s %s avg saving takes = %d ms", full_name, StrategyToNameMap[type], lapse_milli_seconds);
    os_reg.close();
  }
  switch (type) {
    case STRATEGY_REG: {
      if (target->double_regret_ != nullptr) {
        SaveAssertMap<DOUBLE_REGRET>(target, type, prefix);
      } else {
        SaveAssertMap<INT_REGRET>(target, type, prefix);
      }
      break;
    }
    case STRATEGY_WAVG: {
      if (target->ulong_wavg_ != nullptr) {
        SaveAssertMap<ULONG_WAVG>(target, type, prefix);
      } else {
        SaveAssertMap<UINT_WAVG>(target, type, prefix);
      }
      break;
    }
    case STRATEGY_ZIPAVG:SaveAssertMap<ZIPAVG>(target, type, prefix);
      break;
    default:
      logger::critical("unsupported strategy type %s", StrategyToNameMap[type]);
  }
}

void LoadStrategy(Strategy *target, STRATEGY_TYPE type, const std::string &prefix, bool disk_lookup) {
  if (disk_lookup && type != STRATEGY_ZIPAVG)
    logger::critical("only support disk loading for zipavg now");

  std::filesystem::path dir(BULLDOG_DIR_DATA_STG);
  //only support scalar mode load strategy. it is a hack cuz we dont quite load reg now
  if (disk_lookup) {
    logger::debug("strategy %s will read from disk", prefix);
    target->file_ptr = new std::ifstream(dir / (prefix + "." + StrategyToNameMap[type]), std::ios::binary);
    if (!target->file_ptr->is_open()){
      logger::critical("can not open strategy file %s", prefix);
    }
  } else {
    target->AllocateMemory(STRATEGY_ZIPAVG, CFR_SCALAR_SOLVE);
    auto begin = std::chrono::steady_clock::now();
    std::filesystem::path file(prefix + "." + StrategyToNameMap[type]);
    if (!std::filesystem::exists(dir / file)) {
      logger::critical("missing %s", dir / file);
    }
    std::ifstream is(dir / file);
    cereal::BinaryInputArchive archive(is);
    RNBA size = target->ag_->kernel_->MaxIndex();
    try {
      switch (type) {
        case STRATEGY_REG:archive(cereal::binary_data(target->double_regret_, sizeof(DOUBLE_REGRET) * size));
          break;
        case STRATEGY_WAVG:archive(cereal::binary_data(target->ulong_wavg_, sizeof(ULONG_WAVG) * size));
          break;
        case STRATEGY_ZIPAVG:archive(cereal::binary_data(target->zipavg_, sizeof(ZIPAVG) * size));
          break;
        default:
          logger::critical("unsupported strategy type %s", StrategyToNameMap[type]);
      }
    } catch (cereal::Exception &e) {
      logger::critical("cereal exit with exception:\n %s", e.what());
    }
    is.close();
    auto end = std::chrono::steady_clock::now();
    auto lapse_milli_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    logger::debug("%s loading takes = %d ms", StrategyToNameMap[type], lapse_milli_seconds);
  }
  switch (type) {
    //only support MCCFR large strategy IO
    case STRATEGY_REG:LoadAssertMap<INT_REGRET>(target, type, prefix);
      break;
    case STRATEGY_WAVG:LoadAssertMap<UINT_WAVG>(target, type, prefix);
      break;
    case STRATEGY_ZIPAVG:LoadAssertMap<ZIPAVG>(target, type, prefix);
      break;
    default:
      logger::critical("unsupported strategy type %s", StrategyToNameMap[type]);
  }
}

template<class T>
void SaveAssertMap(Strategy *target, STRATEGY_TYPE type, const std::string &prefix) {
  //scalar mode only
  std::map<RNBA, T> assert_map;
  auto max_index = target->ag_->kernel_->MaxIndex();
  if (max_index > 10000000)
    max_index = 10000000;
  for (int i = 0; i < 10; i++) {
    auto rnd_1 = GenRndNumber(0, max_index - 1);
    switch (type) {
      case STRATEGY_REG:assert_map[rnd_1] = target->int_regret_[rnd_1];
        break;
      case STRATEGY_WAVG:assert_map[rnd_1] = target->uint_wavg_[rnd_1];
        break;
      case STRATEGY_ZIPAVG:assert_map[rnd_1] = target->zipavg_[rnd_1];
        break;
      default:
        logger::critical("unsupported strategy type %s", StrategyToNameMap[type]);
    }
  }

  std::filesystem::path dir(BULLDOG_DIR_DATA_STG);
  auto check_file = dir / (prefix + ".check_" + StrategyToNameMap[type]);
  std::ofstream os(check_file, std::ios::binary | std::ios::trunc);
  if (os.is_open()) {
    cereal::JSONOutputArchive archive(os);
    try {
      archive(assert_map);
    } catch (cereal::Exception &e) {
      logger::critical("cereal exit with exception:\n %s", e.what());
    }
  } else {
    auto e = std::system_error(errno, std::system_category());
    logger::critical("failed to open %s: %s (system:%d)", check_file, e.what(), e.code());
  }
  os.close();
  logger::debug("save checkpoint file for %s", StrategyToNameMap[type]);
}

template<class T>
void LoadAssertMap(Strategy *target, STRATEGY_TYPE type, const std::string &prefix) {
  std::map<RNBA, T> assert_map;
  std::filesystem::path dir(BULLDOG_DIR_DATA_STG);
  std::ifstream is(dir / (prefix + ".check_" + StrategyToNameMap[type]), std::ios::binary);
  if (is.is_open()) {
    cereal::JSONInputArchive archive(is);
    archive(assert_map);
  } else {
    logger::error("unable to open %s", dir / (prefix + ".check_" + StrategyToNameMap[type]));
  }
  is.close();

  std::ifstream ds(dir / (prefix + "." + StrategyToNameMap[type]), std::ios::in);
  if (!ds.is_open())
    logger::error("nononnnoo");

  for (auto const &[key, val] : assert_map) {
    switch (type) {
      case STRATEGY_REG:
        if (target->int_regret_[key] != (INT_REGRET) val) {
          logger::critical("%s assert value error [%d] != %f", StrategyToNameMap[type], key, val);
        }
        break;
      case STRATEGY_WAVG:
        if (target->uint_wavg_[key] != (UINT_WAVG) val) {
          logger::critical("%s assert value error [%d] != %d", StrategyToNameMap[type], key, val);
        }
        break;
      case STRATEGY_ZIPAVG:
        if (target->GetZipAvg(key) != val) {
          logger::critical("%s assert value error [%d] != %d", StrategyToNameMap[type], key, val);
        }
        break;
      default:
        logger::critical("unsupported strategy type %s", StrategyToNameMap[type]);
    }
  }
}
