#include "fetcher/record_fetcher.h"
#include "config/fetcher_config.h"
#include <httplib.h>
#include <glog/logging.h>
#include <base64.h>
#include <zlib.h>
#include <sstream>
#include <vector>

namespace {

bool decode_script_to_json(const std::string& script, nlohmann::json& out) {
  try {
    std::string decoded = base64_decode(script, true);

    if (decoded.empty()) {
      LOG(ERROR) << "Base64 decoded script is empty";
      return false;
    }

    z_stream stream{};
    stream.next_in  = reinterpret_cast<Bytef*>(decoded.data());
    stream.avail_in = static_cast<uInt>(decoded.size());

    if (inflateInit(&stream) != Z_OK) {
      LOG(ERROR) << "Failed to initialize zlib";
      return false;
    }

    std::vector<uint8_t> decompressed;
    std::array<uint8_t, 32768> buffer{};

    int ret;
    do {
      stream.next_out  = buffer.data();
      stream.avail_out = static_cast<uInt>(buffer.size());

      ret = inflate(&stream, Z_NO_FLUSH);
      if (ret != Z_OK && ret != Z_STREAM_END) {
        LOG(ERROR) << "zlib decompression error: " << ret;
        inflateEnd(&stream);
        return false;
      }

      size_t produced = buffer.size() - stream.avail_out;
      decompressed.insert(
          decompressed.end(), buffer.begin(), buffer.begin() + produced);
    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);

    std::string decompressed_str(decompressed.begin(), decompressed.end());
    out = nlohmann::json::parse(decompressed_str);
    return true;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to decode script: " << e.what();
    return false;
  }
}

} // namespace

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

    std::string key = output_key.empty() ? "record/" + record_id : output_key;

    json record_data;
    try {
      record_data = json::parse(response->body);
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to parse record JSON: " << e.what();
      return false;
    }

    if (record_data.contains("script") && record_data["script"].is_string()) {
      json script_json;
      if (decode_script_to_json(record_data["script"], script_json)) {
        record_data["step"] = script_json;
        LOG(INFO) << "Parsed script and added step field";
      } else {
        LOG(WARNING) << "Script parsing failed; step not added";
      }
    } else {
      LOG(WARNING) << "Record JSON missing script field";
    }

    if (!storage_->save_json(key, record_data)) {
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
