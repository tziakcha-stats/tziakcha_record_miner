#include "analyzer/record_parser.h"
#include "utils/script_decoder.h"
#include <glog/logging.h>

namespace tziakcha {
namespace analyzer {

RecordParser::RecordParser() : is_valid_(false) {}

RecordParser::~RecordParser() = default;

bool RecordParser::Parse(const std::string& record_json_str) {
  try {
    json record_json = json::parse(record_json_str);

    if (!DecodeAndParseScript(record_json_str)) {
      LOG(ERROR) << "Failed to decode and parse script";
      return false;
    }

    if (script_data_.contains("g")) {
      game_config_ = script_data_["g"];
    }

    if (script_data_.contains("p")) {
      player_info_ = script_data_["p"];
    }

    if (script_data_.contains("y")) {
      auto y_data = script_data_["y"];
      if (y_data.is_array()) {
        for (size_t i = 0; i < 4; ++i) {
          if (i < y_data.size()) {
            win_data_.push_back(y_data[i]);
          } else {
            win_data_.push_back(json::object());
          }
        }
      }
    }

    ParseActions();
    is_valid_ = true;
    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Parse error: " << e.what();
    is_valid_ = false;
    return false;
  }
}

bool RecordParser::DecodeAndParseScript(const std::string& record_json_str) {
  try {
    json record_json = json::parse(record_json_str);
    if (!record_json.contains("script")) {
      LOG(ERROR) << "Script field not found in record";
      return false;
    }

    std::string script_encoded = record_json["script"];
    if (!utils::DecodeScriptToJson(script_encoded, script_data_)) {
      LOG(ERROR) << "Failed to decode script";
      return false;
    }

    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to decode and parse script: " << e.what();
    return false;
  }
}

void RecordParser::ParseActions() {
  actions_.clear();

  if (!script_data_.contains("a")) {
    return;
  }

  auto acts_data = script_data_["a"];
  if (!acts_data.is_array()) {
    return;
  }

  for (const auto& act_array : acts_data) {
    if (!act_array.is_array() || act_array.size() < 3) {
      continue;
    }

    int combined    = act_array[0].get<int>();
    int player_idx  = (combined >> 4) & 3;
    int action_type = combined & 15;
    int data        = act_array[1].get<int>();
    int time        = act_array[2].get<int>();

    actions_.push_back({player_idx, action_type, data, time});
  }
}

const json& RecordParser::GetScriptData() const { return script_data_; }

const std::vector<Action>& RecordParser::GetActions() const { return actions_; }

const json& RecordParser::GetGameConfig() const { return game_config_; }

const json& RecordParser::GetPlayerInfo() const { return player_info_; }

const json& RecordParser::GetWinData(int player_idx) const {
  if (player_idx >= 0 && player_idx < static_cast<int>(win_data_.size())) {
    return win_data_[player_idx];
  }
  static json empty;
  return empty;
}

bool RecordParser::IsValid() const { return is_valid_; }

} // namespace analyzer
} // namespace tziakcha
