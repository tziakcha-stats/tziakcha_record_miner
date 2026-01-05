#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "storage/storage.h"

namespace tziakcha {
namespace fetcher {

class LocalHistoryFilter {
public:
  static bool filter_by_date(const std::string& input_key,
                             const std::string& date,
                             const std::string& output_key);

  static bool filter_by_date(storage::Storage& storage,
                             const std::string& input_key,
                             const std::string& date,
                             const std::string& output_key);
};

} // namespace fetcher
} // namespace tziakcha
