#include "fetcher/record_fetcher.h"
#include "fetcher/record_parser.h"
#include "config/fetcher_config.h"
#include <httplib.h>
#include <glog/logging.h>
#include <sstream>

namespace tziakcha {
namespace fetcher {

RecordFetcher::RecordFetcher(std::shared_ptr<storage::Storage> storage)
    : storage_(storage) {}

bool RecordFetcher::fetch_record(const std::string& record_id,
                                 const std::string& output_key) {
  auto& config = config::FetcherConfig::instance();

  std::string base_url = config.get_base_url();
  std::string endpoint = config.get_record_endpoint();
  int timeout          = config.get_timeout_ms();
  bool use_ssl         = config.use_ssl();
  auto headers         = config.get_headers();

  std::string payload = "id=" + record_id;

  LOG(INFO) << "Fetching record: " << record_id;

  try {
    httplib::Result response;

    if (use_ssl) {
      httplib::SSLClient client(base_url, 443);
      client.enable_server_certificate_verification(false);
      client.set_connection_timeout(timeout / 1000);
      client.set_read_timeout(timeout / 1000);
      client.set_write_timeout(timeout / 1000);

      httplib::Headers http_headers;
      for (const auto& [key, value] : headers) {
        http_headers.insert({key, value});
      }

      response = client.Post(endpoint, http_headers, payload, "text/plain");
    } else {
      httplib::Client client(base_url);
      client.set_connection_timeout(timeout / 1000);
      client.set_read_timeout(timeout / 1000);
      client.set_write_timeout(timeout / 1000);

      httplib::Headers http_headers;
      for (const auto& [key, value] : headers) {
        http_headers.insert({key, value});
      }

      response = client.Post(endpoint, http_headers, payload, "text/plain");
    }

    if (!response) {
      LOG(ERROR) << "Failed to fetch record " << record_id
                 << ": Connection error";
      return false;
    }

    if (response->status != 200) {
      LOG(ERROR) << "Failed to fetch record " << record_id << ": HTTP status "
                 << response->status;
      return false;
    }

    std::string key = output_key.empty() ? "origin/" + record_id : output_key;

    json record_data;
    try {
      record_data = json::parse(response->body);
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to parse record JSON: " << e.what();
      return false;
    }

    json processed_record = RecordParser::merge_record_with_script(record_data);

    if (!storage_->save_json(key, processed_record)) {
      LOG(ERROR) << "Failed to save record " << record_id << " to storage";
      return false;
    }

    LOG(INFO) << "Successfully saved record " << record_id << " to " << key;
    return true;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception while fetching record " << record_id << ": "
               << e.what();
    return false;
  }
}

} // namespace fetcher
} // namespace tziakcha
