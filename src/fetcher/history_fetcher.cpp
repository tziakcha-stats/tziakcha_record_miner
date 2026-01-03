#include "fetcher/history_fetcher.h"
#include "config/fetcher_config.h"
#include "storage/filesystem_storage.h"
#include <chrono>
#include <ctime>
#include <httplib.h>
#include <fstream>
#include <glog/logging.h>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>

namespace tziakcha {
namespace fetcher {

namespace {
std::optional<HistoryFetcher::DateRangeMs>
build_date_range(const std::string& start_date, const std::string& end_date) {
  if (start_date.empty() && end_date.empty()) {
    return std::nullopt;
  }

  if (start_date.size() != 8 || end_date.size() != 8) {
    LOG(ERROR) << "Date format must be YYYYMMDD";
    return std::nullopt;
  }

  auto parse_ymd =
      [](const std::string& ymd, bool end_of_day) -> std::optional<int64_t> {
    for (char c : ymd) {
      if (c < '0' || c > '9') {
        return std::nullopt;
      }
    }

    std::tm tm = {};
    tm.tm_year = std::stoi(ymd.substr(0, 4)) - 1900;
    tm.tm_mon  = std::stoi(ymd.substr(4, 2)) - 1;
    tm.tm_mday = std::stoi(ymd.substr(6, 2));
    tm.tm_hour = end_of_day ? 23 : 0;
    tm.tm_min  = end_of_day ? 59 : 0;
    tm.tm_sec  = end_of_day ? 59 : 0;

    std::time_t tt = std::mktime(&tm);
    if (tt == -1) {
      return std::nullopt;
    }

    int64_t ms = static_cast<int64_t>(tt) * 1000;
    if (end_of_day) {
      ms += 999; // include the entire last second
    }
    return ms;
  };

  auto start_ms = parse_ymd(start_date, false);
  auto end_ms   = parse_ymd(end_date, true);

  if (!start_ms || !end_ms) {
    LOG(ERROR) << "Failed to parse start/end date";
    return std::nullopt;
  }

  if (*start_ms > *end_ms) {
    LOG(ERROR) << "Start date must be earlier than or equal to end date";
    return std::nullopt;
  }

  return HistoryFetcher::DateRangeMs{*start_ms, *end_ms};
}
} // namespace

HistoryFetcher::HistoryFetcher(std::shared_ptr<storage::Storage> storage)
    : storage_(storage) {
  if (!storage_) {
    storage_ = std::make_shared<storage::FileSystemStorage>("data");
  }
}

bool HistoryFetcher::fetch_page(
    const std::string& url,
    const std::string& headers,
    int page,
    const std::optional<DateRangeMs>& date_range,
    bool& reached_range_end) {
  auto& config = config::FetcherConfig::instance();

  std::string body = (page > 0) ? ("p=" + std::to_string(page)) : "";

  LOG(INFO) << "Fetching page " << (page + 1) << ", body: '" << body << "'";

  httplib::Headers header_map;
  for (const auto& [key, value] : config.get_headers()) {
    header_map.emplace(key, value);
  }
  header_map.emplace("Cookie", headers);

  httplib::Result result;

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  if (config.use_ssl()) {
    httplib::SSLClient cli(config.get_base_url(), 443);
    cli.enable_server_certificate_verification(false);
    cli.set_follow_location(true);
    cli.set_connection_timeout(config.get_timeout_ms() / 1000);
    cli.set_read_timeout(config.get_timeout_ms() / 1000);
    cli.set_write_timeout(config.get_timeout_ms() / 1000);

    LOG(INFO) << "Using HTTPS to " << config.get_base_url() << ":443"
              << config.get_history_endpoint();

    result = cli.Post(
        config.get_history_endpoint(),
        header_map,
        body,
        "text/plain;charset=UTF-8");
  } else {
#endif
    httplib::Client cli(config.get_base_url(), 80);
    cli.set_follow_location(true);
    cli.set_connection_timeout(config.get_timeout_ms() / 1000);
    cli.set_read_timeout(config.get_timeout_ms() / 1000);
    cli.set_write_timeout(config.get_timeout_ms() / 1000);

    LOG(INFO) << "Using HTTP to " << config.get_base_url() << ":80"
              << config.get_history_endpoint();

    result = cli.Post(
        config.get_history_endpoint(),
        header_map,
        body,
        "text/plain;charset=UTF-8");
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  }
#endif

  if (!result) {
    auto err = result.error();
    LOG(ERROR) << "Request failed: no result returned";
    LOG(ERROR) << "Error code: " << static_cast<int>(err);
    LOG(ERROR) << "Error message: " << httplib::to_string(err);
    return false;
  }

  if (result->status != 200) {
    LOG(ERROR) << "Request failed with status: " << result->status;
    LOG(ERROR) << "Response body: " << result->body;
    return false;
  }

  LOG(INFO) << "Request succeeded with status 200, response size: "
            << result->body.size();

  try {
    auto data = json::parse(result->body);
    if (data.is_object() && data.contains("games") &&
        data["games"].is_array()) {
      size_t game_count = data["games"].size();
      LOG(INFO) << "Found " << game_count << " games on page " << (page + 1);

      size_t added_count = 0;
      int64_t min_start  = std::numeric_limits<int64_t>::max();

      for (const auto& game : data["games"]) {
        int64_t start_ms = 0;
        bool has_start   = false;
        if (game.contains("start_time") &&
            (game["start_time"].is_number_integer() ||
             game["start_time"].is_number())) {
          start_ms  = game["start_time"].get<int64_t>();
          has_start = true;
          if (start_ms < min_start) {
            min_start = start_ms;
          }
        }

        if (date_range) {
          if (!has_start) {
            continue;
          }

          if (start_ms < date_range->start_ms) {
            reached_range_end = true;
          }

          if (start_ms < date_range->start_ms ||
              start_ms > date_range->end_ms) {
            continue;
          }
        }

        records_.push_back(game);
        added_count++;
      }

      if (!date_range) {
        reached_range_end = false;
      } else if (min_start != std::numeric_limits<int64_t>::max() &&
                 min_start < date_range->start_ms) {
        reached_range_end = true;
      }

      LOG(INFO) << "Added " << added_count << " records from page "
                << (page + 1);
      return true;
    }

    LOG(WARNING) << "Response JSON does not contain 'games' array";
    LOG(WARNING) << "Response: " << result->body.substr(0, 500);
    return false;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to parse JSON response: " << e.what();
    LOG(ERROR) << "Response body: " << result->body.substr(0, 500);
    return false;
  }
}

bool HistoryFetcher::save_records(const std::string& key) const {
  json output(records_);
  if (!storage_->save_json(key, output)) {
    LOG(ERROR) << "Failed to save records to storage with key: " << key;
    return false;
  }
  LOG(INFO) << "Saved " << records_.size()
            << " records to storage key: " << key;
  return true;
}

bool HistoryFetcher::fetch(const std::string& cookie,
                           const std::string& key,
                           const std::string& start_date,
                           const std::string& end_date) {
  auto& config = config::FetcherConfig::instance();

  records_.clear();

  auto date_range = build_date_range(start_date, end_date);
  if (!date_range && (!start_date.empty() || !end_date.empty())) {
    LOG(ERROR) << "Invalid date range; aborting fetch";
    return false;
  }

  LOG(INFO) << "Starting to fetch history records";
  LOG(INFO) << "Max pages to fetch: " << config.get_max_pages();

  std::string url = config.get_base_url() + config.get_history_endpoint();

  for (int page = 1; page <= config.get_max_pages(); ++page) {
    int page_param         = (page > 1) ? (page - 1) : 0;
    bool reached_range_end = false;
    if (!fetch_page(url, cookie, page_param, date_range, reached_range_end)) {
      LOG(WARNING) << "Failed to fetch page " << page << ", continuing...";
      continue;
    }

    if (date_range && reached_range_end) {
      LOG(INFO)
          << "Reached start of requested date range; stopping early at page "
          << page;
      break;
    }
  }

  LOG(INFO) << "Finished fetching. Total records: " << records_.size();

  return save_records(key);
}

std::vector<json>
HistoryFetcher::filter_by_title(const std::string& keyword) const {
  std::vector<json> filtered;
  for (const auto& record : records_) {
    if (record.contains("title") && record["title"].is_string()) {
      const auto& title = record["title"].get<std::string>();
      if (title.find(keyword) != std::string::npos) {
        filtered.push_back(record);
      }
    }
  }
  return filtered;
}

} // namespace fetcher
} // namespace tziakcha
