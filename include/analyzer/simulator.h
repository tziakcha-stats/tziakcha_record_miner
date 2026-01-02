#pragma once

#include "analyzer/action.h"
#include "analyzer/game_log.h"
#include "analyzer/game_state.h"
#include "analyzer/record_parser.h"
#include "analyzer/win_analyzer.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace tziakcha {
namespace analyzer {

struct SimulationResult {
  bool success;
  WinAnalysis win_analysis;
  GameLog game_log;
  std::string error_message;
};

class RecordSimulator {
public:
  RecordSimulator();

  SimulationResult Simulate(const std::string& record_json_str);

  const GameLog& GetGameLog() const;
  const std::vector<StepLog>& GetStepLogs() const;

private:
  RecordParser parser_;
  GameState state_;
  ActionProcessor processor_;
  WinAnalyzer analyzer_;
  GameLog game_log_;
  std::vector<StepLog> step_logs_;
  bool winner_set_from_actions_ = false;

  void ProcessGameInfoAndSetup();
  void ProcessAllActions();
  void ExtractWinInfoFromScript();
  void LogGameInfo();
  void LogAction(int step_number,
                 const Action& action,
                 int time_elapsed_ms,
                 const std::string& action_desc);
  StepLog BuildStepLog(int step_number,
                       const Action& action,
                       int time_elapsed_ms,
                       const std::string& action_desc);
  std::string BuildActionDescription(const Action& action);
  std::vector<std::string> GetPlayerHandStrings(int player_idx) const;
  std::vector<std::string> GetPlayerPackStrings(int player_idx) const;
  std::vector<std::string> GetPlayerDiscardStrings(int player_idx) const;
};

} // namespace analyzer
} // namespace tziakcha
