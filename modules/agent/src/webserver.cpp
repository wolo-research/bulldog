//
//  Created by Ivan Mejia on 12/24/16.
//
// MIT License
//
// Copyright (c) 2016 ivmeroLabs.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include <iostream>

#include "bulldog_controller.hpp"
#include "usr_interrupt_handler.hpp"
#include "runtime_utils.hpp"
#include "server_constants.h"

#include <bulldog/logger.hpp>
#include <cxxopts.hpp>

using namespace web;
using namespace cfx;

int main(int argc, char *argv[]) {
  cxxopts::Options options("Bulldog server", "Provide bulldog api service");
  options.add_options()
      ("l,log_level", "log level", cxxopts::value<std::string>()->default_value("info"))
      ("f,log_file", "file?", cxxopts::value<std::string>()->default_value("bulldog_server"))
      ("e,endpoint", "server address", cxxopts::value<std::string>()->default_value("host_auto_ip4"))
      ("g,game", "game file", cxxopts::value<std::string>())
      ("n,engine", "engine file", cxxopts::value<std::string>())
      ("h,help", "print usage information");
  auto result = options.parse(argc, argv);
  if (result.count("help") || (result.arguments()).empty()) {
    std::cout << options.help() << std::endl;
    exit(0);
  }

  bool check_must_opts = result.count("engine")
      && result.count("game");
  if (!check_must_opts) {
    std::cout << "fill the must-have options" << std::endl;
    std::cout << options.help() << std::endl;
    exit(EXIT_FAILURE);
  }

  /*
   * logger
   */
  if (result.count("log_file")) {
    std::filesystem::path dir(BULLDOG_DIR_LOG);
    auto name = result["log_file"].as<std::string>();
    logger::init_logger(dir / name, result["log_level"].as<std::string>());
  } else {
    logger::init_logger(result["log_level"].as<std::string>());
  }

  InterruptHandler::hookSIGINT();

  BulldogController server;

  auto ep = "http://" + result["endpoint"].as<std::string>() + ":8080/v1/bulldog/api";
  server.setEndpoint(ep);
  server.LoadDefault(result["engine"].as<std::string>(), result["game"].as<std::string>());

  try {
    // wait for server initialization...
    server.accept().wait();
    logger::info("listing for requests at:%s ", server.endpoint());

    InterruptHandler::waitForUserInterrupt();
    server.shutdown().wait();
    logger::info("server shutting down");
  }
  catch (std::exception &e) {
    logger::error("%s", e.what());
  }
  catch (...) {
    RuntimeUtils::printStackTrace();
  }
  return 0;
}
