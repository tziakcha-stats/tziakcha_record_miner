#include "fetcher/history_fetcher.h"
#include "fetcher/session_fetcher.h"
#include "fetcher/record_fetcher.h"
#include "config/fetcher_config.h"
#include "storage/filesystem_storage.h"
#include <cxxopts.hpp>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <glog/logging.h>

namespace fs = std::filesystem;

void print_usage() {
  std::cout << "Tziakcha Record Miner - Fetcher CLI\n\n";
  std::cout << "Usage:\n";
  std::cout << "  fetcher_cli <command> [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  history     Fetch game history from server\n";
  std::cout << "  sessions    Fetch session records from history\n";
  std::cout << "  session     Fetch a single session by ID\n";
  std::cout << "  record      Fetch a single record by ID\n";
  std::cout << "  records     Batch fetch records from session JSON\n";
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
      "start-date",
      "Start date (YYYYMMDD)",
      cxxopts::value<std::string>()->default_value(""))(
      "end-date",
      "End date (YYYYMMDD)",
      cxxopts::value<std::string>()->default_value(""))(
      "f,filter", "Filter by title keyword", cxxopts::value<std::string>())(
      "p,print",
      "Print JSON to console after fetching",
      cxxopts::value<bool>()->default_value("false"))("h,help", "Print help");

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
  std::string start_date  = result["start-date"].as<std::string>();
  std::string end_date    = result["end-date"].as<std::string>();

  if (!start_date.empty() || !end_date.empty()) {
    if (start_date.empty() || end_date.empty()) {
      std::cerr << "Error: both --start-date and --end-date are required"
                << std::endl;
      return 1;
    }

    if (storage_key == "history/history") {
      storage_key += "_" + start_date + "_" + end_date;
    }
  }

  auto storage =
      std::make_shared<tziakcha::storage::FileSystemStorage>(data_dir);

  tziakcha::fetcher::HistoryFetcher fetcher(storage);

  if (!fetcher.fetch(cookie, storage_key, start_date, end_date)) {
    LOG(ERROR) << "Failed to fetch history records";
    return 1;
  }

  LOG(INFO) << "History records saved to storage key: " << storage_key;

  if (result["print"].as<bool>()) {
    std::cout << "\n--- History JSON ---\n";
    storage->print_json(storage_key);
  }

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
          "sessions/record_parent_map"))(
      "p,print",
      "Print grouped sessions JSON to console",
      cxxopts::value<bool>()->default_value("false"))("h,help", "Print help");

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

  if (result["print"].as<bool>()) {
    std::cout << "\n--- Grouped Sessions JSON ---\n";
    storage->print_json(output_key);
  }

  return 0;
}

int cmd_session(int argc, char* argv[]) {
  cxxopts::Options options(
      "fetcher_cli session", "Fetch a single session by ID");
  options.add_options()(
      "c,config",
      "Path to configuration file",
      cxxopts::value<std::string>()->default_value(
          "config/fetcher_config.json"))(
      "d,data-dir",
      "Data storage directory",
      cxxopts::value<std::string>()->default_value("data"))(
      "s,session-id",
      "Session ID to fetch (e.g., TszL5UsT)",
      cxxopts::value<std::string>())(
      "o,output", "Output key for session data", cxxopts::value<std::string>())(
      "p,print",
      "Print JSON to console after fetching",
      cxxopts::value<bool>()->default_value("false"))("h,help", "Print help");

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (!result.count("session-id")) {
    std::cerr << "Error: Session ID is required (use -s or --session-id)"
              << std::endl;
    std::cout << options.help() << std::endl;
    return 1;
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

  std::string session_id = result["session-id"].as<std::string>();
  std::string data_dir   = result["data-dir"].as<std::string>();
  std::string output_key =
      result.count("output") ? result["output"].as<std::string>()
                             : "sessions/" + session_id;

  auto storage =
      std::make_shared<tziakcha::storage::FileSystemStorage>(data_dir);

  tziakcha::fetcher::SessionFetcher fetcher(storage);

  if (!fetcher.fetch_single_session(session_id, output_key)) {
    LOG(ERROR) << "Failed to fetch session: " << session_id;
    return 1;
  }

  std::cout << "Successfully fetched session " << session_id << " to "
            << output_key << std::endl;

  if (result["print"].as<bool>()) {
    std::cout << "\n--- Session JSON ---\n";
    storage->print_json(output_key);
  }

  return 0;
}

int cmd_record(int argc, char* argv[]) {
  cxxopts::Options options("fetcher_cli record", "Fetch a single record by ID");
  options.add_options()(
      "c,config",
      "Path to configuration file",
      cxxopts::value<std::string>()->default_value(
          "config/fetcher_config.json"))(
      "d,data-dir",
      "Data storage directory",
      cxxopts::value<std::string>()->default_value("data"))(
      "r,record-id", "Record ID to fetch", cxxopts::value<std::string>())(
      "o,output", "Output key for record data", cxxopts::value<std::string>())(
      "p,print",
      "Print JSON to console after fetching",
      cxxopts::value<bool>()->default_value("false"))("h,help", "Print help");

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (!result.count("record-id")) {
    std::cerr << "Error: Record ID is required (use -r or --record-id)"
              << std::endl;
    std::cout << options.help() << std::endl;
    return 1;
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

  std::string record_id = result["record-id"].as<std::string>();
  std::string data_dir  = result["data-dir"].as<std::string>();
  std::string output_key =
      result.count("output") ? result["output"].as<std::string>()
                             : "record/" + record_id;

  auto storage =
      std::make_shared<tziakcha::storage::FileSystemStorage>(data_dir);

  tziakcha::fetcher::RecordFetcher fetcher(storage);

  if (!fetcher.fetch_record(record_id, output_key)) {
    LOG(ERROR) << "Failed to fetch record: " << record_id;
    return 1;
  }

  std::cout << "Successfully fetched record " << record_id << " to "
            << output_key << std::endl;

  if (result["print"].as<bool>()) {
    std::cout << "\n--- Record JSON ---\n";
    storage->print_json(output_key);
  }

  return 0;
}

int cmd_records(int argc, char* argv[]) {
  cxxopts::Options options(
      "fetcher_cli records", "Batch fetch records from session JSON");
  options.add_options()(
      "c,config",
      "Path to configuration file",
      cxxopts::value<std::string>()->default_value(
          "config/fetcher_config.json"))(
      "d,data-dir",
      "Data storage directory",
      cxxopts::value<std::string>()->default_value("data"))(
      "i,input",
      "Input session JSON key (e.g., sessions/all_record)",
      cxxopts::value<std::string>()->default_value("sessions/all_record"))(
      "o,output-dir",
      "Output directory for records",
      cxxopts::value<std::string>()->default_value("record"))(
      "l,limit",
      "Limit number of records to fetch (0 = no limit)",
      cxxopts::value<int>()->default_value("0"))(
      "delay",
      "Delay between requests in milliseconds",
      cxxopts::value<int>()->default_value("500"))(
      "skip-existing",
      "Skip records that already exist in storage",
      cxxopts::value<bool>()->default_value("true"))("h,help", "Print help");

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
  std::string output_dir = result["output-dir"].as<std::string>();
  int limit              = result["limit"].as<int>();
  int delay_ms           = result["delay"].as<int>();
  bool skip_existing     = result["skip-existing"].as<bool>();

  auto storage =
      std::make_shared<tziakcha::storage::FileSystemStorage>(data_dir);

  json session_json;
  if (!storage->load_json(input_key, session_json)) {
    LOG(ERROR) << "Failed to load session JSON from: " << input_key;
    return 1;
  }

  std::vector<std::string> record_ids;
  if (session_json.is_array()) {
    for (const auto& session : session_json) {
      if (session.contains("records") && session["records"].is_array()) {
        for (const auto& record_id : session["records"]) {
          if (record_id.is_string()) {
            record_ids.push_back(record_id.get<std::string>());
          }
        }
      } else if (session.contains("l") && session["l"].is_array()) {
        for (const auto& record_id : session["l"]) {
          if (record_id.is_string()) {
            record_ids.push_back(record_id.get<std::string>());
          }
        }
      }
    }
  } else if (session_json.is_object()) {
    if (session_json.contains("records") &&
        session_json["records"].is_array()) {
      for (const auto& record_id : session_json["records"]) {
        if (record_id.is_string()) {
          record_ids.push_back(record_id.get<std::string>());
        }
      }
    } else if (session_json.contains("l") && session_json["l"].is_array()) {
      for (const auto& record_id : session_json["l"]) {
        if (record_id.is_string()) {
          record_ids.push_back(record_id.get<std::string>());
        }
      }
    }
  }

  if (record_ids.empty()) {
    LOG(WARNING) << "No record IDs found in session JSON";
    return 1;
  }

  LOG(INFO) << "Found " << record_ids.size() << " record IDs";

  if (limit > 0 && limit < static_cast<int>(record_ids.size())) {
    record_ids.resize(limit);
    LOG(INFO) << "Limited to " << limit << " records";
  }

  tziakcha::fetcher::RecordFetcher fetcher(storage);
  int success_count = 0;
  int skip_count    = 0;
  int fail_count    = 0;

  for (size_t i = 0; i < record_ids.size(); ++i) {
    const auto& record_id  = record_ids[i];
    std::string output_key = output_dir + "/" + record_id;

    if (skip_existing && storage->exists(output_key)) {
      LOG(INFO) << "[" << (i + 1) << "/" << record_ids.size()
                << "] Skipping existing record: " << record_id;
      skip_count++;
      continue;
    }

    LOG(INFO) << "[" << (i + 1) << "/" << record_ids.size()
              << "] Fetching record: " << record_id;

    if (fetcher.fetch_record(record_id, output_key)) {
      success_count++;
      LOG(INFO) << "  Successfully fetched: " << record_id;
    } else {
      fail_count++;
      LOG(ERROR) << "  Failed to fetch: " << record_id;
    }

    if (i < record_ids.size() - 1 && delay_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
  }

  std::cout << "\n=== Batch Fetch Summary ===\n";
  std::cout << "Total records: " << record_ids.size() << "\n";
  std::cout << "Successfully fetched: " << success_count << "\n";
  std::cout << "Skipped (existing): " << skip_count << "\n";
  std::cout << "Failed: " << fail_count << "\n";

  return (fail_count > 0) ? 1 : 0;
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
  } else if (command == "session") {
    return cmd_session(argc - 1, argv + 1);
  } else if (command == "record") {
    return cmd_record(argc - 1, argv + 1);
  } else if (command == "records") {
    return cmd_records(argc - 1, argv + 1);
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    std::cerr << "Run 'fetcher_cli help' for usage." << std::endl;
    return 1;
  }
}
