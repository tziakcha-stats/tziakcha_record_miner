#include "fetcher/session_fetcher.h"
#include "config/fetcher_config.h"
#include "storage/filesystem_storage.h"
#include <httplib.h>
#include <sstream>
#include <glog/logging.h>

namespace tziakcha {
namespace fetcher {

SessionFetcher::SessionFetcher(std::shared_ptr<storage::Storage> storage)
    : storage_(storage) {
  if (!storage_) {
    storage_ = std::make_shared<storage::FileSystemStorage>("data");
  }
}

bool SessionFetcher::fetch_session_records(const std::string& session_id,
                                           const std::string& title,
                                           std::vector<std::string>& records) {
  auto& config = config::FetcherConfig::instance();

  httplib::Headers header_map;
  for (const auto& [key, value] : config.get_headers()) {
    header_map.emplace(key, value);
  }

  std::string endpoint = config.get_game_endpoint() + "/?id=" + session_id;

  httplib::Result result;

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  if (config.use_ssl()) {
    httplib::SSLClient cli(config.get_base_url(), 443);
    cli.enable_server_certificate_verification(false);
    cli.set_follow_location(true);
    cli.set_connection_timeout(config.get_timeout_ms() / 1000);
    cli.set_read_timeout(config.get_timeout_ms() / 1000);
    cli.set_write_timeout(config.get_timeout_ms() / 1000);

    result = cli.Post(endpoint, header_map, "", "text/plain;charset=UTF-8");
  } else {
#endif
    httplib::Client cli(config.get_base_url(), 80);
    cli.set_follow_location(true);
    cli.set_connection_timeout(config.get_timeout_ms() / 1000);
    cli.set_read_timeout(config.get_timeout_ms() / 1000);
    cli.set_write_timeout(config.get_timeout_ms() / 1000);

    result = cli.Post(endpoint, header_map, "", "text/plain;charset=UTF-8");
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  }
#endif

  if (!result) {
    LOG(ERROR) << "Failed to fetch session " << session_id << ": no result";
    return false;
  }

  if (result->status != 200) {
    LOG(ERROR) << "Failed to fetch session " << session_id
               << ", status: " << result->status;
    return false;
  }

  try {
    auto data = json::parse(result->body);
    if (data.is_object() && data.contains("records") &&
        data["records"].is_array()) {
      int idx = 0;
      for (const auto& record : data["records"]) {
        if (record.is_object() && record.contains("i")) {
          std::string rec_id = record["i"].get<std::string>();
          records.push_back(rec_id);

          RecordParentInfo parent_info;
          parent_info.session_id       = session_id;
          parent_info.title            = title;
          parent_info.order_in_session = idx + 1;
          record_parent_map_[rec_id]   = parent_info;

          idx++;
        }
      }
      return true;
    } else {
      LOG(WARNING) << "Session " << session_id
                   << " response does not contain 'records' array";
      return false;
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to parse session " << session_id
               << " response: " << e.what();
    return false;
  }
}

bool SessionFetcher::fetch_sessions(const std::string& history_key) {
  grouped_sessions_.clear();
  record_parent_map_.clear();

  json history_data;
  if (!storage_->load_json(history_key, history_data)) {
    LOG(ERROR) << "Failed to load history from: " << history_key;
    return false;
  }

  if (!history_data.is_array()) {
    LOG(ERROR) << "History data is not an array";
    return false;
  }

  LOG(INFO) << "Processing " << history_data.size() << " history items";

  int success_count = 0;
  int failed_count  = 0;

  for (const auto& item : history_data) {
    if (!item.is_object()) {
      continue;
    }

    std::string session_id = item.value("id", "");
    std::string title      = item.value("title", "");

    if (session_id.empty()) {
      continue;
    }

    std::vector<std::string> records;
    if (fetch_session_records(session_id, title, records)) {
      SessionRecords session;
      session.session_id = session_id;
      session.title      = title;
      session.records    = records;
      grouped_sessions_.push_back(session);

      LOG(INFO) << "Fetched session " << session_id << " with "
                << records.size() << " records";
      success_count++;
    } else {
      LOG(WARNING) << "Failed to fetch session " << session_id;
      failed_count++;
    }
  }

  LOG(INFO) << "Finished fetching sessions. Success: " << success_count
            << ", Failed: " << failed_count;
  LOG(INFO) << "Total records: " << record_parent_map_.size();

  return success_count > 0;
}

bool SessionFetcher::save_results(const std::string& grouped_key,
                                  const std::string& map_key) {
  json grouped_json = json::array();
  for (const auto& session : grouped_sessions_) {
    json session_json;
    session_json["session_id"] = session.session_id;
    session_json["title"]      = session.title;
    session_json["records"]    = session.records;
    grouped_json.push_back(session_json);
  }

  if (!storage_->save_json(grouped_key, grouped_json)) {
    LOG(ERROR) << "Failed to save grouped sessions to: " << grouped_key;
    return false;
  }

  json map_json;
  for (const auto& [rec_id, parent_info] : record_parent_map_) {
    json info_json;
    info_json["session_id"]       = parent_info.session_id;
    info_json["title"]            = parent_info.title;
    info_json["order_in_session"] = parent_info.order_in_session;
    map_json[rec_id]              = info_json;
  }

  if (!storage_->save_json(map_key, map_json)) {
    LOG(ERROR) << "Failed to save record parent map to: " << map_key;
    return false;
  }

  LOG(INFO) << "Saved " << grouped_sessions_.size() << " grouped sessions and "
            << record_parent_map_.size() << " record mappings";

  return true;
}

bool SessionFetcher::fetch_single_session(const std::string& session_id,
                                          const std::string& output_key) {
  LOG(INFO) << "Fetching single session: " << session_id;

  std::vector<std::string> records;
  if (!fetch_session_records(session_id, "", records)) {
    LOG(ERROR) << "Failed to fetch session records for: " << session_id;
    return false;
  }

  json session_data;
  session_data["session_id"]   = session_id;
  session_data["records"]      = records;
  session_data["record_count"] = records.size();

  if (!storage_->save_json(output_key, session_data)) {
    LOG(ERROR) << "Failed to save session data to: " << output_key;
    return false;
  }

  LOG(INFO) << "Saved session " << session_id << " with " << records.size()
            << " records to: " << output_key;

  return true;
}

} // namespace fetcher
} // namespace tziakcha
