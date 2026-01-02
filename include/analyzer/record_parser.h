#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace tziakcha {
namespace analyzer {

struct Action {
  int player_idx;
  int action_type;
  int data;
  int time_ms;
};

struct WinInfo {
  int winner_idx;
  int win_tile;
  bool is_self_drawn;
};

class RecordParser {
public:
  RecordParser();
  ~RecordParser();

  bool Parse(const std::string& record_json_str);

  const json& GetScriptData() const;
  const std::vector<Action>& GetActions() const;
  const json& GetGameConfig() const;
  const json& GetPlayerInfo() const;
  const json& GetWinData(int player_idx) const;

  bool IsValid() const;

private:
  json script_data_;
  std::vector<Action> actions_;
  json game_config_;
  json player_info_;
  std::vector<json> win_data_;
  bool is_valid_;

  bool DecodeAndParseScript(const std::string& record_json_str);
  void ParseActions();
};

} // namespace analyzer
} // namespace tziakcha
