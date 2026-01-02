#pragma once

#include "storage/storage.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace tziakcha {
namespace storage {

class FileSystemStorage : public Storage {
public:
  explicit FileSystemStorage(const std::string& base_dir = "data");

  bool save_json(const std::string& key, const json& data) override;
  bool load_json(const std::string& key, json& data) override;
  bool exists(const std::string& key) override;
  bool remove(const std::string& key) override;
  std::vector<std::string> list_keys(const std::string& prefix) override;

  std::string get_base_dir() const { return base_dir_.string(); }

private:
  fs::path base_dir_;

  fs::path key_to_path(const std::string& key) const;
  std::string path_to_key(const fs::path& path) const;
};

} // namespace storage
} // namespace tziakcha
