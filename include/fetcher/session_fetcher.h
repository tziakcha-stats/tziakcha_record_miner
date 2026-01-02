#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include "storage/storage.h"

using json = nlohmann::json;

namespace tziakcha {
namespace fetcher {

struct RecordParentInfo {
  std::string session_id;
  std::string title;
  int order_in_session;
};

struct SessionRecords {
  std::string session_id;
  std::string title;
  std::vector<std::string> records;
};

class SessionFetcher {
public:
  explicit SessionFetcher(std::shared_ptr<storage::Storage> storage = nullptr);

  bool fetch_sessions(const std::string& history_key = "history/history");

  const std::vector<SessionRecords>& get_grouped_sessions() const {
    return grouped_sessions_;
  }

  const std::map<std::string, RecordParentInfo>& get_record_parent_map() const {
    return record_parent_map_;
  }

  bool save_results(const std::string& grouped_key = "sessions/all_record",
                    const std::string& map_key = "sessions/record_parent_map");

private:
  std::shared_ptr<storage::Storage> storage_;
  std::vector<SessionRecords> grouped_sessions_;
  std::map<std::string, RecordParentInfo> record_parent_map_;

  bool fetch_session_records(const std::string& session_id,
                             const std::string& title,
                             std::vector<std::string>& records);
};

} // namespace fetcher
} // namespace tziakcha
