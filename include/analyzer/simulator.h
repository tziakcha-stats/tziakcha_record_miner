#pragma once

#include "analyzer/action.h"
#include "analyzer/game_log.h"
#include "analyzer/game_state.h"
#include "analyzer/record_parser.h"
#include "analyzer/win_analyzer.h"
#include <array>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

#include <map>

namespace tziakcha {
namespace analyzer {

struct GameStateSnapshot {
  std::array<std::vector<int>, 4> final_hands;
  std::array<std::vector<std::vector<int>>, 4> packs;
  std::array<std::vector<int>, 4> pack_directions;
  std::array<std::vector<int>, 4> discards;
  std::array<std::vector<int>, 4> flower_tiles;
  std::array<int, 4> flower_counts{};
};

struct SimulationResult {
  bool success;
  WinAnalysis win_analysis;
  GameLog game_log;
  std::string error_message;
  std::map<int, std::vector<int>> starting_hands;
  GameStateSnapshot game_state_snapshot;
};

class RecordSimulator {
public:
  RecordSimulator();

  SimulationResult Simulate(const std::string& record_json_str);

  using ActionObserver =
      std::function<void(const Action&, int step_number, const GameState&)>;

  void AddActionObserver(ActionObserver observer);

  void ClearActionObservers();

  const GameLog& GetGameLog() const;
  const std::vector<StepLog>& GetStepLogs() const;

  int GetRoundWindIndex() const;

private:
  RecordParser parser_;
  GameState state_;
  ActionProcessor processor_;
  WinAnalyzer analyzer_;
  GameLog game_log_;
  std::vector<StepLog> step_logs_;
  bool winner_set_from_actions_ = false;

  std::map<int, std::vector<int>> starting_hands_;

  std::vector<ActionObserver> action_observers_;

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
