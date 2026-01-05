#include "storage/filesystem_storage.h"

#include "fetcher/local_history_filter.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace tziakcha {
namespace fetcher {

bool LocalHistoryFilter::filter_by_date(const std::string& input_path,
                                        const std::string& date,
                                        const std::string& output_path) {
  tziakcha::storage::FileSystemStorage storage("data");
  return filter_by_date(storage, input_path, date, output_path);
}

bool LocalHistoryFilter::filter_by_date(
    tziakcha::storage::Storage& storage,
    const std::string& input_key,
    const std::string& date,
    const std::string& output_key) {
  json all;
  if (!storage.load_json(input_key, all)) {
    std::cerr << "Failed to load input json: " << input_key << std::endl;
    return false;
  }
  std::vector<json> filtered;
  for (const auto& rec : all) {
    if (rec.contains("start_time")) {
      int64_t start_ms = 0;
      if (rec["start_time"].is_number_integer() ||
          rec["start_time"].is_number()) {
        start_ms      = rec["start_time"].get<int64_t>();
        std::time_t t = start_ms / 1000;
        std::tm* tm   = std::localtime(&t);
        char buf[9];
        std::strftime(buf, sizeof(buf), "%Y%m%d", tm);
        if (date == buf) {
          filtered.push_back(rec);
        }
      }
    }
  }
  if (!storage.save_json(output_key, json(filtered))) {
    std::cerr << "Failed to save output json: " << output_key << std::endl;
    return false;
  }
  return true;
}

bool LocalHistoryFilter::filter_by_date_range(
    const std::string& input_path,
    const std::string& start_date,
    const std::string& end_date,
    const std::string& output_path) {
  tziakcha::storage::FileSystemStorage storage("data");
  return filter_by_date_range(
      storage, input_path, start_date, end_date, output_path);
}

bool LocalHistoryFilter::filter_by_date_range(
    tziakcha::storage::Storage& storage,
    const std::string& input_key,
    const std::string& start_date,
    const std::string& end_date,
    const std::string& output_key) {
  json all;
  if (!storage.load_json(input_key, all)) {
    std::cerr << "Failed to load input json: " << input_key << std::endl;
    return false;
  }

  if (start_date.size() != 8 || end_date.size() != 8) {
    std::cerr << "Date format must be YYYYMMDD" << std::endl;
    return false;
  }

  auto parse_date = [](const std::string& ymd, bool end_of_day) -> int64_t {
    std::tm tm     = {};
    tm.tm_year     = std::stoi(ymd.substr(0, 4)) - 1900;
    tm.tm_mon      = std::stoi(ymd.substr(4, 2)) - 1;
    tm.tm_mday     = std::stoi(ymd.substr(6, 2));
    tm.tm_hour     = end_of_day ? 23 : 0;
    tm.tm_min      = end_of_day ? 59 : 0;
    tm.tm_sec      = end_of_day ? 59 : 0;
    std::time_t tt = std::mktime(&tm);
    int64_t ms     = static_cast<int64_t>(tt) * 1000;
    if (end_of_day) {
      ms += 999;
    }
    return ms;
  };

  int64_t start_ms = parse_date(start_date, false);
  int64_t end_ms   = parse_date(end_date, true);

  std::vector<json> filtered;
  for (const auto& rec : all) {
    if (rec.contains("start_time")) {
      int64_t start_time = 0;
      if (rec["start_time"].is_number_integer() ||
          rec["start_time"].is_number()) {
        start_time = rec["start_time"].get<int64_t>();
        if (start_time >= start_ms && start_time <= end_ms) {
          filtered.push_back(rec);
        }
      }
    }
  }

  if (!storage.save_json(output_key, json(filtered))) {
    std::cerr << "Failed to save output json: " << output_key << std::endl;
    return false;
  }
  return true;
}

} // namespace fetcher
} // namespace tziakcha
