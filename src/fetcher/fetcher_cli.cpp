#include "fetcher/history_fetcher.h"
#include "fetcher/session_fetcher.h"
#include "config/fetcher_config.h"
#include "storage/filesystem_storage.h"
#include <cxxopts.hpp>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <glog/logging.h>

namespace fs = std::filesystem;

void print_usage() {
  std::cout << "Tziakcha Record Miner - Fetcher CLI\n\n";
  std::cout << "Usage:\n";
  std::cout << "  fetcher_cli <command> [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  history     Fetch game history from server\n";
  std::cout << "  sessions    Fetch session records from history\n";
  std::cout << "  help        Show this help message\n\n";
  std::cout << "Run 'fetcher_cli <command> --help' for more information on a "
               "command.\n";
}

int cmd_history(int argc, char* argv[]) {
  cxxopts::Options options(
      "fetcher_cli history", "Fetch game history from server");
  options.add_options()(
      "c,config",
      "Path to configuration file",
      cxxopts::value<std::string>()->default_value(
          "config/fetcher_config.json"))(
      "d,data-dir",
      "Data storage directory",
      cxxopts::value<std::string>()->default_value("data"))(
      "k,key",
      "Storage key for history",
      cxxopts::value<std::string>()->default_value("history/history"))(
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

  std::string data_dir    = result["data-dir"].as<std::string>();
  std::string storage_key = result["key"].as<std::string>();

  auto storage =
      std::make_shared<tziakcha::storage::FileSystemStorage>(data_dir);

  tziakcha::fetcher::HistoryFetcher fetcher(storage);

  if (!fetcher.fetch(cookie, storage_key)) {
    LOG(ERROR) << "Failed to fetch history records";
    return 1;
  }

  LOG(INFO) << "History records saved to storage key: " << storage_key;

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

int cmd_sessions(int argc, char* argv[]) {
  cxxopts::Options options(
      "fetcher_cli sessions", "Fetch session records from history");
  options.add_options()(
      "c,config",
      "Path to configuration file",
      cxxopts::value<std::string>()->default_value(
          "config/fetcher_config.json"))(
      "d,data-dir",
      "Data storage directory",
      cxxopts::value<std::string>()->default_value("data"))(
      "i,input",
      "Input history key",
      cxxopts::value<std::string>()->default_value("history/history"))(
      "o,output",
      "Output grouped sessions key",
      cxxopts::value<std::string>()->default_value("sessions/all_record"))(
      "m,map",
      "Output record parent map key",
      cxxopts::value<std::string>()->default_value(
          "sessions/record_parent_map"))("h,help", "Print help");

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

  std::string data_dir   = result["data-dir"].as<std::string>();
  std::string input_key  = result["input"].as<std::string>();
  std::string output_key = result["output"].as<std::string>();
  std::string map_key    = result["map"].as<std::string>();

  auto storage =
      std::make_shared<tziakcha::storage::FileSystemStorage>(data_dir);

  tziakcha::fetcher::SessionFetcher fetcher(storage);

  if (!fetcher.fetch_sessions(input_key)) {
    LOG(ERROR) << "Failed to fetch session records";
    return 1;
  }

  if (!fetcher.save_results(output_key, map_key)) {
    LOG(ERROR) << "Failed to save session results";
    return 1;
  }

  std::cout << "Written grouped sessions to " << output_key << " (sessions="
            << fetcher.get_grouped_sessions().size() << ")" << std::endl;
  std::cout << "Written record parent map to " << map_key << " (records="
            << fetcher.get_record_parent_map().size() << ")" << std::endl;

  return 0;
}

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

  if (argc < 2) {
    print_usage();
    return 1;
  }

  std::string command = argv[1];

  if (command == "help" || command == "--help" || command == "-h") {
    print_usage();
    return 0;
  } else if (command == "history") {
    return cmd_history(argc - 1, argv + 1);
  } else if (command == "sessions") {
    return cmd_sessions(argc - 1, argv + 1);
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    std::cerr << "Run 'fetcher_cli help' for usage." << std::endl;
    return 1;
  }
}
