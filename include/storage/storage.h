#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace tziakcha {
namespace storage {

class Storage {
public:
  virtual ~Storage() = default;

  virtual bool save_json(const std::string& key, const json& data)      = 0;
  virtual bool load_json(const std::string& key, json& data)            = 0;
  virtual bool exists(const std::string& key)                           = 0;
  virtual bool remove(const std::string& key)                           = 0;
  virtual std::vector<std::string> list_keys(const std::string& prefix) = 0;
  virtual void print_json(const std::string& key, int indent = 2)       = 0;
};

} // namespace storage
} // namespace tziakcha
