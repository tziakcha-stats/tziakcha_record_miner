#include "storage/filesystem_storage.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <glog/logging.h>

namespace tziakcha {
namespace storage {

FileSystemStorage::FileSystemStorage(const std::string& base_dir)
    : base_dir_(base_dir) {
  if (!fs::exists(base_dir_)) {
    if (!fs::create_directories(base_dir_)) {
      LOG(ERROR) << "Failed to create base directory: " << base_dir;
    }
  }
}

fs::path FileSystemStorage::key_to_path(const std::string& key) const {
  fs::path path = base_dir_;

  size_t last_pos = 0;
  size_t pos      = 0;

  while ((pos = key.find('/', last_pos)) != std::string::npos) {
    std::string part = key.substr(last_pos, pos - last_pos);
    if (!part.empty()) {
      path /= part;
    }
    last_pos = pos + 1;
  }

  std::string filename = key.substr(last_pos);
  if (!filename.empty()) {
    path /= filename;
  }

  if (path.extension() != ".json") {
    path.replace_extension(".json");
  }

  return path;
}

std::string FileSystemStorage::path_to_key(const fs::path& path) const {
  std::string rel = fs::relative(path, base_dir_).string();

  const std::string json_ext = ".json";
  if (rel.length() >= json_ext.length() &&
      rel.compare(
          rel.length() - json_ext.length(), json_ext.length(), json_ext) == 0) {
    rel.erase(rel.length() - json_ext.length());
  }

  for (auto& c : rel) {
    if (c == '\\')
      c = '/';
  }

  return rel;
}

bool FileSystemStorage::save_json(const std::string& key, const json& data) {
  fs::path path = key_to_path(key);

  if (!fs::exists(path.parent_path())) {
    if (!fs::create_directories(path.parent_path())) {
      LOG(ERROR) << "Failed to create directory: " << path.parent_path();
      return false;
    }
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file for writing: " << path;
    return false;
  }

  file << data.dump();
  file.close();

  LOG(INFO) << "Saved JSON to: " << path;
  return true;
}

bool FileSystemStorage::load_json(const std::string& key, json& data) {
  fs::path path = key_to_path(key);

  if (!fs::exists(path)) {
    LOG(WARNING) << "File does not exist: " << path;
    return false;
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file for reading: " << path;
    return false;
  }

  try {
    file >> data;
    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to parse JSON from " << path << ": " << e.what();
    return false;
  }
}

bool FileSystemStorage::exists(const std::string& key) {
  fs::path path = key_to_path(key);
  return fs::exists(path);
}

bool FileSystemStorage::remove(const std::string& key) {
  fs::path path = key_to_path(key);

  if (!fs::exists(path)) {
    LOG(WARNING) << "File does not exist: " << path;
    return false;
  }

  try {
    fs::remove(path);
    LOG(INFO) << "Removed file: " << path;
    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to remove file " << path << ": " << e.what();
    return false;
  }
}

std::vector<std::string>
FileSystemStorage::list_keys(const std::string& prefix) {
  std::vector<std::string> keys;
  fs::path prefix_path = key_to_path(prefix);

  if (!fs::exists(prefix_path.parent_path())) {
    return keys;
  }

  try {
    for (const auto& entry :
         fs::recursive_directory_iterator(prefix_path.parent_path())) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        std::string key = path_to_key(entry.path());
        if (key.find(prefix) == 0) {
          keys.push_back(key);
        }
      }
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to list keys with prefix " << prefix << ": "
               << e.what();
  }

  return keys;
}

void FileSystemStorage::print_json(const std::string& key, int indent) {
  json data;
  if (load_json(key, data)) {
    std::cout << data.dump(indent) << std::endl;
  } else {
    LOG(ERROR) << "Failed to load JSON for key: " << key;
  }
}

} // namespace storage
} // namespace tziakcha
