#include "fetcher/record_parser.h"
#include <glog/logging.h>
#include <zlib.h>
#include <cstring>

namespace tziakcha {
namespace fetcher {

json RecordParser::parse_script(const std::string& encoded_script) {
  try {
    std::string decoded;
    const char* b64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int val  = 0;
    int valb = -6;

    for (unsigned char c : encoded_script) {
      if (c == '=')
        break;

      const char* p = std::strchr(b64_chars, c);
      if (p == nullptr)
        continue;

      val = (val << 6) + (p - b64_chars);
      valb += 6;

      if (valb >= 0) {
        decoded.push_back((val >> valb) & 0xFF);
        valb -= 8;
      }
    }

    if (valb > -6) {
      decoded.push_back((val << (8 - valb - 6)) & 0xFF);
    }

    std::vector<uint8_t> decompressed;
    const int chunk_size = 32768;
    uint8_t out[chunk_size];

    z_stream stream;
    stream.zalloc   = Z_NULL;
    stream.zfree    = Z_NULL;
    stream.opaque   = Z_NULL;
    stream.avail_in = 0;
    stream.next_in  = Z_NULL;

    int ret = inflateInit(&stream);
    if (ret != Z_OK) {
      LOG(ERROR) << "Failed to initialize zlib decompression";
      return json::object();
    }

    stream.avail_in = decoded.size();
    stream.next_in  = (uint8_t*)decoded.data();

    do {
      stream.avail_out = chunk_size;
      stream.next_out  = out;

      ret = inflate(&stream, Z_NO_FLUSH);
      if (ret != Z_OK && ret != Z_STREAM_END) {
        inflateEnd(&stream);
        LOG(ERROR) << "zlib decompression error: " << ret;
        return json::object();
      }

      size_t produced = chunk_size - stream.avail_out;
      decompressed.insert(decompressed.end(), out, out + produced);
    } while (ret != Z_STREAM_END);

    inflateEnd(&stream);

    std::string decompressed_str(decompressed.begin(), decompressed.end());

    return json::parse(decompressed_str);

  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to parse script: " << e.what();
    return json::object();
  }
}

json RecordParser::merge_record_with_script(const json& record_json) {
  json result = record_json;

  if (!record_json.contains("script") || !record_json["script"].is_string()) {
    LOG(WARNING) << "Record JSON missing or invalid script field";
    return result;
  }

  try {
    json script_data = parse_script(record_json["script"]);

    if (script_data.is_object() && !script_data.empty()) {
      result["step"] = script_data;
      LOG(INFO) << "Successfully parsed and merged script data";
    } else {
      LOG(WARNING) << "Parsed script data is empty or invalid";
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception while merging script: " << e.what();
  }

  return result;
}

} // namespace fetcher
} // namespace tziakcha
