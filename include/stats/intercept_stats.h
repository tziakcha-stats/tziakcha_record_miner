#pragma once

#include "analyzer/game_state.h"
#include "calc/fan_calculator.h"
#include <string>
#include <vector>

namespace tziakcha {
namespace stats {

struct InterceptEvent {
  int step_number;
  int discarder_idx;
  int winner_idx;
  int discard_tile;
  std::vector<int> potential_winners;
  std::vector<int> potential_fans;
  bool is_intercept;

  std::string ToString() const;
};

struct InterceptStatsResult {
  int total_ron_wins;
  int intercept_count;
  double intercept_rate;
  std::vector<InterceptEvent> events;

  std::string ToSummary() const;
};

class InterceptStats {
public:
  InterceptStats();

  void SetRoundId(const std::string& round_id);

  InterceptEvent CheckIntercept(
      int discarder_idx,
      int discard_tile,
      const analyzer::GameState& game_state,
      int dealer_idx,
      int round_wind_index,
      int step_number);

  void AddEvent(const InterceptEvent& event);

  InterceptStatsResult GetResult() const;

  void Reset();

private:
  std::vector<InterceptEvent> events_;
  std::string current_round_id_;

  int CalculateWinFan(int player_idx,
                      int win_tile,
                      const analyzer::GameState& game_state,
                      int dealer_idx,
                      int round_wind_index) const;

  std::string BuildHandtilesString(
      const std::vector<int>& hand,
      const std::vector<std::vector<int>>& packs,
      const std::vector<int>& pack_dirs,
      int win_tile,
      bool is_self_drawn,
      int player_idx,
      int dealer_idx,
      char round_wind_char,
      const analyzer::GameState& game_state) const;

  std::vector<int> GetWinPriorityOrder(int discarder_idx) const;
};

} // namespace stats
} // namespace tziakcha
