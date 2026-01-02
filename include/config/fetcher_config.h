#pragma once

#include <string>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace tziakcha {
namespace config {

class FetcherConfig {
public:
  static FetcherConfig& instance();

  bool load(const std::string& config_file);

  std::string get_base_url() const;
  bool use_ssl() const;
  std::string get_history_endpoint() const;
  std::string get_game_endpoint() const;
  std::string get_record_endpoint() const;
  int get_timeout_ms() const;

  const std::map<std::string, std::string>& get_headers() const;

  int get_max_pages() const;
  std::string get_output_file() const;

private:
  FetcherConfig();

  json config_;
  std::map<std::string, std::string> headers_;
  bool loaded_;
};

} // namespace config
} // namespace tziakcha
