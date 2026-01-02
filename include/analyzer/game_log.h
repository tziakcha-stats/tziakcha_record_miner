#pragma once

#include "analyzer/win_analyzer.h"
#include <string>
#include <vector>

namespace tziakcha {
namespace analyzer {

struct StepLog {
  int step_number;
  int player_idx;
  std::string player_name;
  std::string player_wind;
  int action_type;
  std::string action_description;
  int time_elapsed_ms;
  std::vector<std::string> hand_tiles;
  std::vector<std::string> pack_tiles;
  std::vector<std::string> discard_tiles;
};

struct GameLog {
  std::string game_title;
  std::vector<std::string> player_names;
  int dealer_idx;
  std::vector<StepLog> step_logs;
  WinAnalysis win_analysis;
};

} // namespace analyzer
} // namespace tziakcha
