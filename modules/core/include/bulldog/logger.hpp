//
// Created by Carmen C on 24/3/2020.
//

#ifndef BULLDOG_MODULES_CORE_SRC_LOGGER_HPP_
#define BULLDOG_MODULES_CORE_SRC_LOGGER_HPP_

#include <string>
#include <map>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/bundled/printf.h>

enum class LOGLEVEL {
  TRACE,
  DEBUG,
  INFO,
  WARN,
  ERROR,
  CRITICAL,
  OFF
};

static std::map<std::string, LOGLEVEL> LevelMap{
    {"trace", LOGLEVEL::TRACE},
    {"debug", LOGLEVEL::DEBUG},
    {"info", LOGLEVEL::INFO},
    {"warn", LOGLEVEL::WARN},
    {"error", LOGLEVEL::ERROR},
    {"critical", LOGLEVEL::CRITICAL},
    {"off", LOGLEVEL::OFF},
};

static void set_logger(LOGLEVEL l) {
//#undef SPDLOG_ACTIVE_LEVEL
//#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
  auto log = spdlog::stderr_color_mt("console");
  spdlog::set_default_logger(log);
  spdlog::set_pattern("%Y-%m-%d %H:%M:%S|%t|%l|%v");
  switch (l) {
    case LOGLEVEL::TRACE: spdlog::set_level(spdlog::level::trace);
      break;
    case LOGLEVEL::DEBUG:
      spdlog::set_level(spdlog::level::debug);
      break;
    case LOGLEVEL::INFO: spdlog::set_level(spdlog::level::info);
      break;
    case LOGLEVEL::WARN: spdlog::set_level(spdlog::level::warn);
      break;
    case LOGLEVEL::ERROR: spdlog::set_level(spdlog::level::err);
      break;
    case LOGLEVEL::CRITICAL: spdlog::set_level(spdlog::level::critical);
      break;
    case LOGLEVEL::OFF: spdlog::set_level(spdlog::level::off);
      break;
  }
}

static void set_logger(const std::string &log_filename, LOGLEVEL l) {
  auto log = spdlog::daily_logger_mt("daily_logger",
                                     log_filename,
                                     0,
                                     0);
  log->flush_on(spdlog::level::info);
  spdlog::set_default_logger(log);
  spdlog::set_pattern("%Y-%m-%d %H:%M:%S|%t|%l|%v");
  switch (l) {
    case LOGLEVEL::TRACE: spdlog::set_level(spdlog::level::trace);
      break;
    case LOGLEVEL::DEBUG:
      spdlog::set_level(spdlog::level::debug);
      log->flush_on(spdlog::level::debug);
      break;
    case LOGLEVEL::INFO: spdlog::set_level(spdlog::level::info);
      break;
    case LOGLEVEL::WARN: spdlog::set_level(spdlog::level::warn);
      break;
    case LOGLEVEL::ERROR: spdlog::set_level(spdlog::level::err);
      break;
    case LOGLEVEL::CRITICAL: spdlog::set_level(spdlog::level::critical);
      break;
    case LOGLEVEL::OFF: spdlog::set_level(spdlog::level::off);
      break;
  }
}

static void log(LOGLEVEL l, const std::string &msg) {
  switch (l) {
    case LOGLEVEL::TRACE: spdlog::trace(msg);
      break;
    case LOGLEVEL::DEBUG: spdlog::debug(msg);
      break;
    case LOGLEVEL::INFO: spdlog::info(msg);
      break;
    case LOGLEVEL::WARN: spdlog::warn(msg);
      break;
    case LOGLEVEL::ERROR: spdlog::error(msg);
      break;
    case LOGLEVEL::CRITICAL: spdlog::critical(msg);
      break;
    case LOGLEVEL::OFF: break;
  }
}

template<typename... Args>
static void log(LOGLEVEL l, const std::string &s, const Args &... args) {
  switch (l) {
    case LOGLEVEL::TRACE: spdlog::trace(fmt::sprintf(s, args...));
      break;
    case LOGLEVEL::DEBUG: spdlog::debug(fmt::sprintf(s, args...));
      break;
    case LOGLEVEL::INFO: spdlog::info(fmt::sprintf(s, args...));
      break;
    case LOGLEVEL::WARN: spdlog::warn(fmt::sprintf(s, args...));
      break;
    case LOGLEVEL::ERROR: spdlog::error(fmt::sprintf(s, args...));
      break;
    case LOGLEVEL::CRITICAL: spdlog::critical(fmt::sprintf(s, args...));
      break;
    case LOGLEVEL::OFF: break;
  }
}

namespace logger {
static inline void init_logger(const std::string &loglevel = "info") {
  if (LevelMap.count(loglevel) > 0) {
    set_logger(LevelMap[loglevel]);
  }
}
static inline void init_logger(const std::string &log_filename, const std::string &loglevel) {
  if (LevelMap.count(loglevel) > 0) {
    set_logger(log_filename, LevelMap[loglevel]);
  }
}
template<typename... Args>
static void trace(const std::string &s, const Args &... args) {
  if (sizeof...(args)) {
    log(LOGLEVEL::TRACE, s, args...);
  } else {
    log(LOGLEVEL::TRACE, s);
  }
}
template<typename... Args>
static void debug(const std::string &s, const Args &... args) {
  if (sizeof...(args)) {
    log(LOGLEVEL::DEBUG, s, args...);
  } else {
    log(LOGLEVEL::DEBUG, s);
  }
}
template<typename... Args>
static void info(const std::string &s, const Args &... args) {
  if (sizeof...(args)) {
    log(LOGLEVEL::INFO, s, args...);
  } else {
    log(LOGLEVEL::INFO, s);
  }
}
template<typename... Args>
static void warn(const std::string &s, const Args &... args) {
  if (sizeof...(args)) {
    log(LOGLEVEL::WARN, s, args...);
  } else {
    log(LOGLEVEL::WARN, s);
  }
}
template<typename... Args>
static void error(const std::string &s, const Args &... args) {
  if (sizeof...(args)) {
    log(LOGLEVEL::ERROR, s, args...);
  } else {
    log(LOGLEVEL::ERROR, s);
  }
}

template<typename... Args>
static void critical(const std::string &s, const Args &... args) {
  if (sizeof...(args)) {
    log(LOGLEVEL::CRITICAL, s, args...);
  } else {
    log(LOGLEVEL::CRITICAL, s);
  }
  exit(EXIT_FAILURE);
}
static inline void breaker() {
  debug("==========================================================================");
}

template<typename... Args>
static void require_warn(bool b, const std::string &s, const Args &... args) {
  if (!b) warn(s, args...);
}

template<typename... Args>
static void require_error(bool b, const std::string &s, const Args &... args) {
  if (!b) error(s, args...);
}

template<typename... Args>
static void require_critical(bool b, const std::string &s, const Args &... args) {
  if (!b) critical(s, args...);
}
}

#endif //BULLDOG_MODULES_CORE_SRC_LOGGER_HPP_
