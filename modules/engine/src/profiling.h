//
// Created by Isaac Zhang on 3/25/20.
//

#ifndef BULLDOG_MODULES_ENGINE_SRC_PROFILING_H_
#define BULLDOG_MODULES_ENGINE_SRC_PROFILING_H_

#include "node.h"
#include "hand_belief.h"
#include <bulldog/logger.hpp>

#include <fstream>
#include <iostream>
#include <filesystem>
#include <mutex>
#include <type_traits>

namespace fs = std::filesystem;

using ExplTuple = std::tuple<int,int,double, double, double, double, double>;
static std::string expl_tuple_header = "iter,br,avg,max,min,+dev,-dev\n";
static std::string expl_tuple_ext = ".expl";

/*
 * a generic print method for numeric types
 */
template<class Tuple>
decltype(auto) NumericTupleToCsv(Tuple const &t) {
  auto addcomma = [](auto const ... e) -> decltype(auto) {
    return ((std::to_string(e) + ",")  +...);
  };
  std::string s = std::apply(addcomma, t);
  s.pop_back();
  s += "\n";
  return s;
}

struct sProfilingWriter {
  bool cfr_profiler_header_written = false;
  std::string prefix_ = "default_lab";

  /*
   * create then append mode.
   */
  void WriteToFile(ExplTuple &tuple) {
    bool no_header = !cfr_profiler_header_written;
    {
      fs::path dir(BULLDOG_DIR_DATA_LAB);
      std::string name = prefix_ + expl_tuple_ext;
      fs::path filename(name);
      std::ofstream file;
      if (no_header) {
        file.open(dir / filename); // overwrite
      } else {
        file.open(dir / filename, std::ios_base::app); // append
      }
      if (!file.is_open()) logger::error("lab output file can not open. error");
      if (no_header)
        file << expl_tuple_header;
      file << NumericTupleToCsv(tuple);
      file.close();
    }
    //set the flag
    if (no_header)
      cfr_profiler_header_written = true;
  }
};
#endif //BULLDOG_MODULES_ENGINE_SRC_PROFILING_H_
