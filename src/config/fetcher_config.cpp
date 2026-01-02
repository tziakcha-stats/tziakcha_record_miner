#include "config/fetcher_config.h"
#include <fstream>
#include <sstream>

namespace tziakcha {
namespace config {

FetcherConfig::FetcherConfig() : loaded_(false) {}

FetcherConfig& FetcherConfig::instance() {
  static FetcherConfig config;
  return config;
}

bool FetcherConfig::load(const std::string& config_file) {
  std::ifstream file(config_file);
  if (!file.is_open()) {
    return false;
  }

  try {
    file >> config_;

    if (config_.contains("headers") && config_["headers"].is_object()) {
      for (auto& [key, value] : config_["headers"].items()) {
        std::string header_key = key;
        for (auto& c : header_key) {
          if (c == '_')
            c = '-';
          else if (c >= 'a' && c <= 'z' &&
                   (header_key.empty() || header_key[0] == c)) {
            c = c - 'a' + 'A';
          }
        }

        std::string header_name;
        bool capitalize_next = true;
        for (char c : key) {
          if (c == '_') {
            header_name += '-';
            capitalize_next = true;
          } else if (capitalize_next) {
            header_name += (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
            capitalize_next = false;
          } else {
            header_name += c;
          }
        }

        headers_[header_name] = value.get<std::string>();
      }
    }

    loaded_ = true;
    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

std::string FetcherConfig::get_base_url() const {
  if (!loaded_ || !config_.contains("http"))
    return "";
  return config_["http"].value("base_url", "");
}

bool FetcherConfig::use_ssl() const {
  if (!loaded_ || !config_.contains("http"))
    return true;
  return config_["http"].value("use_ssl", true);
}

std::string FetcherConfig::get_history_endpoint() const {
  if (!loaded_ || !config_.contains("http"))
    return "";
  return config_["http"].value("history_endpoint", "");
}

std::string FetcherConfig::get_game_endpoint() const {
  if (!loaded_ || !config_.contains("http"))
    return "";
  return config_["http"].value("game_endpoint", "");
}

std::string FetcherConfig::get_record_endpoint() const {
  if (!loaded_ || !config_.contains("http"))
    return "";
  return config_["http"].value("record_endpoint", "");
}

int FetcherConfig::get_timeout_ms() const {
  if (!loaded_ || !config_.contains("http"))
    return 30000;
  return config_["http"].value("timeout_ms", 30000);
}

const std::map<std::string, std::string>& FetcherConfig::get_headers() const {
  return headers_;
}

int FetcherConfig::get_max_pages() const {
  if (!loaded_ || !config_.contains("fetcher"))
    return 100;
  return config_["fetcher"].value("max_pages", 100);
}

std::string FetcherConfig::get_output_file() const {
  if (!loaded_ || !config_.contains("fetcher"))
    return "record_lists.json";
  return config_["fetcher"].value("output_file", "record_lists.json");
}

} // namespace config
} // namespace tziakcha
