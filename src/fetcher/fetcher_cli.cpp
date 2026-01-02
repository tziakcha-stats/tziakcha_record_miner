#include "fetcher/history_fetcher.h"
#include "config/fetcher_config.h"
#include <cxxopts.hpp>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <glog/logging.h>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

  cxxopts::Options options(
      "fetcher_cli", "Tziakcha record miner - history fetcher");
  options.add_options()(
      "c,config",
      "Path to configuration file",
      cxxopts::value<std::string>()->default_value(
          "config/fetcher_config.json"))(
      "o,output", "Output file path", cxxopts::value<std::string>())(
      "f,filter", "Filter by title keyword", cxxopts::value<std::string>())(
      "h,help", "Print help");

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  std::string config_file = result["config"].as<std::string>();
  if (!fs::exists(config_file)) {
    std::cerr << "Error: Configuration file not found: " << config_file
              << std::endl;
    return 1;
  }

  auto& config = tziakcha::config::FetcherConfig::instance();
  if (!config.load(config_file)) {
    std::cerr << "Error: Failed to load configuration file" << std::endl;
    return 1;
  }

  const char* cookie = std::getenv("TZI_HISTORY_COOKIE");
  if (!cookie) {
    std::cerr << "Error: TZI_HISTORY_COOKIE environment variable not set"
              << std::endl;
    std::cerr << "Please login at https://tziakcha.net/history/ and get the "
                 "cookie value"
              << std::endl;
    return 1;
  }

  std::string output_file =
      result.count("output") ? result["output"].as<std::string>()
                             : config.get_output_file();

  tziakcha::fetcher::HistoryFetcher fetcher;

  if (!fetcher.fetch(cookie, output_file)) {
    LOG(ERROR) << "Failed to fetch history records";
    return 1;
  }

  LOG(INFO) << "History records saved to " << output_file;

  if (result.count("filter")) {
    std::string keyword = result["filter"].as<std::string>();
    auto filtered       = fetcher.filter_by_title(keyword);
    std::cout << "Filtered results (" << filtered.size()
              << " records):" << std::endl;
    for (const auto& record : filtered) {
      std::cout << record.dump(2) << std::endl;
    }
  }

  return 0;
}
