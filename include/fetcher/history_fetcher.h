#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "storage/storage.h"

using json = nlohmann::json;

namespace tziakcha {
namespace fetcher {

class HistoryFetcher {
public:
  struct DateRangeMs {
    int64_t start_ms;
    int64_t end_ms;
  };

  explicit HistoryFetcher(std::shared_ptr<storage::Storage> storage = nullptr);

  bool fetch(const std::string& cookie,
             const std::string& key        = "history/records",
             const std::string& start_date = "",
             const std::string& end_date   = "");

  const std::vector<json>& get_records() const { return records_; }

  std::vector<json> filter_by_title(const std::string& keyword) const;

private:
  std::vector<json> records_;
  std::shared_ptr<storage::Storage> storage_;

  bool fetch_page(const std::string& url,
                  const std::string& headers,
                  int page,
                  const std::optional<DateRangeMs>& date_range,
                  bool& reached_range_end);
  bool save_records(const std::string& key) const;
};

} // namespace fetcher
} // namespace tziakcha
