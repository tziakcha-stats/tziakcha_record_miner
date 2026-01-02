#include "utils/script_decoder.h"
#include <base64.h>
#include <glog/logging.h>
#include <zlib.h>
#include <array>
#include <vector>

namespace tziakcha {
namespace utils {

bool DecodeScriptToJson(const std::string& encoded, json& out) {
  try {
    std::string decoded = base64_decode(encoded, true);
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
    out = json::parse(decompressed_str);
    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to decode script: " << e.what();
    return false;
  }
}

} // namespace utils
} // namespace tziakcha
