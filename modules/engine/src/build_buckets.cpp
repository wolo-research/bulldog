//
// Created by Carmen C on 18/3/2020.
//

#include "kmeans.h"
#include "builder.hpp"
#include <bulldog/logger.hpp>

#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>
#include <map>

std::map<std::string, BaseBuilder::BuildMode> BuildModeMap{
    {"kmeans-emd", BaseBuilder::BuildKMeansEMD},
    {"kmeans-euclidean", BaseBuilder::BuildKMeansEuclidean},
    {"colex", BaseBuilder::BuildColex},
    {"hierarchical", BaseBuilder::BuildHierarchical},
};

int main(int argc, char *argv[]) {
  cxxopts::Options options("Bucket Builder", "Builds bucket according to settings");
  options.add_options()
      ("m,mode","building mode",cxxopts::value<std::string>(),"[kmeans-emd, kmeans-euclidean, colex, hierarchical]")
      ("p,private_cards", "number of private cards", cxxopts::value<int>(), "[2]")
      ("u,public_cards", "number of public cards, i.e. flop->3, turn->4, river->5", cxxopts::value<int>(), "[3:5]")
      ("c,clusters", "number of bucket clusters", cxxopts::value<int>())
      ("t,threads", "number of threads", cxxopts::value<int>()->default_value("-1"))
      ("i,maxIter", "maximum number of iterrations to run", cxxopts::value<int>()->default_value("100"))
      ("e,error", "ending error", cxxopts::value<double>()->default_value("0.001"))
      ("l,loglevel", "log level", cxxopts::value<std::string>()->default_value("info"), "[debug, info, warn, err]")
      ("o,logfile", "log filename, default prints to console", cxxopts::value<std::string>())
      ("h,help", "print usage information");

  auto result = options.parse(argc, argv);
  if (result.count("help") || (result.arguments()).empty()) {
    std::cout << options.help() << std::endl;
    exit(0);
  }

  bool check_must_opts = result.count("mode")
      && result.count("private_cards")
      && result.count("public_cards")
      && result.count("clusters");
  if (!check_must_opts) {
    std::cout << "fill the must-have options" << std::endl;
    std::cout << options.help() << std::endl;
    exit(EXIT_FAILURE);
  }

  const int priv_cards_num = result["private_cards"].as<int>();
  const int pub_cards_num = result["public_cards"].as<int>();
  const int cluster_num = result["clusters"].as<int>();
  const int max_iterr = result["maxIter"].as<int>();
  const double end_error = result["error"].as<double>();
  int num_threads = result["threads"].as<int>();
  if (num_threads == -1) {
    //use max thread
    num_threads = std::thread::hardware_concurrency();
  }

  std::string loglevel = result["loglevel"].as<std::string>();
  if (result.count("logfile")) {
    std::filesystem::path dir(BULLDOG_DIR_LOG);
    logger::init_logger(dir / result["logfile"].as<std::string>(), loglevel);
  } else {
    logger::init_logger(loglevel);
  }

  BaseBuilder *abs_builder = nullptr;

  switch (BuildModeMap[result["mode"].as<std::string>()]) {
    case BaseBuilder::BuildKMeansEMD:
      abs_builder = new KMeansEMDBuilder("hand_histo_",
                                         priv_cards_num,
                                         pub_cards_num,
                                         cluster_num,
                                         num_threads,
                                         max_iterr,
                                         end_error,
                                         KMeans::InitRandom);
      break;
    case BaseBuilder::BuildKMeansEuclidean:
      abs_builder = new KMeansEuclideanBuilder("equity_",
                                               priv_cards_num,
                                               pub_cards_num,
                                               cluster_num,
                                               num_threads,
                                               max_iterr,
                                               end_error,
                                               KMeans::InitRandom);
      break;
    case BaseBuilder::BuildHierarchical:
      abs_builder = new HierarchicalBuilder(priv_cards_num,
                                            pub_cards_num,
                                            cluster_num,
                                            60,
                                            1500,
                                            num_threads,
                                            max_iterr,
                                            end_error,
                                            KMeans::InitRandom);
      break;
    case BaseBuilder::BuildColex:abs_builder = new ColexBuilder(priv_cards_num, pub_cards_num, cluster_num);
      break;
    default:
      logger::critical("Builder Not Implemented");
  }

  abs_builder->run();
  delete abs_builder;

  exit(EXIT_SUCCESS);
}