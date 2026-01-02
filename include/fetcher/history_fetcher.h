#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace tziakcha {
namespace fetcher {

class HistoryFetcher {
public:
  HistoryFetcher();

  bool fetch(const std::string& cookie,
             const std::string& output_file = "record_lists.json");

  const std::vector<json>& get_records() const { return records_; }

  std::vector<json> filter_by_title(const std::string& keyword) const;

private:
  std::vector<json> records_;
  std::string build_headers(const std::string& cookie) const;
  bool fetch_page(const std::string& url, const std::string& headers, int page);
  bool save_records(const std::string& filename) const;
};

} // namespace fetcher
} // namespace tziakcha
