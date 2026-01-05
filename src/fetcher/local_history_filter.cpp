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

} // namespace fetcher
} // namespace tziakcha
