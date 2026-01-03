#pragma once

#include <string>

namespace tziakcha {
namespace stats {

struct PlayerStatsOptions {
  std::string record_dir       = "data/record";
  std::string output_dir       = "data/player";
  std::string session_map_path = "data/sessions/all_record.json";
  int limit                    = 0;
  bool verbose                 = false;
};

bool RunPlayerStats(const PlayerStatsOptions& options);

} // namespace stats
} // namespace tziakcha
