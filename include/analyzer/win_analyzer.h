#pragma once

#include <array>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace tziakcha {
namespace analyzer {

struct FanDetail {
  int fan_id;
  std::string fan_name;
  int fan_points;
  int count;
};

struct GBFanDetail {
  std::string fan_name;
  int fan_points;
  int count;
};

struct WinAnalysis {
  int winner_idx;
  std::string winner_name;
  std::string winner_wind;
  int total_fan;
  int base_fan;
  int calculated_fan;
  int flower_count;
  std::string formatted_hand;
  std::vector<FanDetail> fan_details;
  std::vector<GBFanDetail> gb_fan_details;
  std::string hand_string_for_gb;
  std::string env_flag;
  std::string gb_handtiles_string;
};

class WinAnalyzer {
public:
  WinAnalyzer();

  void SetWinInfo(int winner_idx, int win_tile, bool is_self_drawn);
  void SetGameState(const class GameState& state);
  void SetScriptData(const json& script_data);

  WinAnalysis Analyze();

  int CalculateFanWithGB(const std::string& gb_string);

  const std::vector<GBFanDetail>& GetGBFanDetails() const {
    return gb_fan_details_;
  }

  bool IsLastCopyTile(int tile) const;
  bool IsSeaLastTile(bool is_self_drawn) const;
  bool IsRobbingKong(bool is_self_drawn) const;

private:
  int winner_idx_;
  int win_tile_;
  bool is_self_drawn_;

  const GameState* state_;
  json script_data_;
  std::vector<GBFanDetail> gb_fan_details_;

  std::string BuildEnvFlag();
  std::string BuildHandStringForGB();
  std::string BuildFormattedHand();
  std::vector<FanDetail> ExtractFanDetails();

  int CountExposedTiles(int tile_base) const;
  std::string GetTileString(int index) const;
  std::string GetWindChar(int player_idx) const;
  std::string GetRoundWindChar() const;
};

} // namespace analyzer
} // namespace tziakcha
