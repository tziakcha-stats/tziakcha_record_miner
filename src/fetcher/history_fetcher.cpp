#include "fetcher/history_fetcher.h"
#include "config/fetcher_config.h"
#include <httplib.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glog/logging.h>

namespace tziakcha {
namespace fetcher {

HistoryFetcher::HistoryFetcher() = default;

bool HistoryFetcher::fetch_page(
    const std::string& url, const std::string& headers, int page) {
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
      for (const auto& game : data["games"]) {
        records_.push_back(game);
      }
      return true;
    } else {
      LOG(WARNING) << "Response JSON does not contain 'games' array";
      LOG(WARNING) << "Response: " << result->body.substr(0, 500);
      return false;
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to parse JSON response: " << e.what();
    LOG(ERROR) << "Response body: " << result->body.substr(0, 500);
    return false;
  }

  return false;
}

bool HistoryFetcher::save_records(const std::string& filename) const {
  std::ofstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file for writing: " << filename;
    return false;
  }

  json output(records_);
  file << output.dump(2);
  file.close();

  LOG(INFO) << "Saved " << records_.size() << " records to " << filename;
  return true;
}

bool HistoryFetcher::fetch(const std::string& cookie,
                           const std::string& output_file) {
  auto& config = config::FetcherConfig::instance();

  records_.clear();

  LOG(INFO) << "Starting to fetch history records";
  LOG(INFO) << "Max pages to fetch: " << config.get_max_pages();

  std::string url = config.get_base_url() + config.get_history_endpoint();

  for (int page = 1; page <= config.get_max_pages(); ++page) {
    int page_param = (page > 1) ? (page - 1) : 0;
    if (!fetch_page(url, cookie, page_param)) {
      LOG(WARNING) << "Failed to fetch page " << page << ", continuing...";
      continue;
    }
  }

  LOG(INFO) << "Finished fetching. Total records: " << records_.size();

  return save_records(output_file);
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
