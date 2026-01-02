#include "analyzer/simulator.h"
#include "base/mahjong_constants.h"
#include "utils/tile.h"
#include <glog/logging.h>
#include <sstream>

namespace tziakcha {
namespace analyzer {

RecordSimulator::RecordSimulator()
    : parser_(), state_(), processor_(state_), analyzer_() {}

SimulationResult RecordSimulator::Simulate(const std::string& record_json_str) {
  SimulationResult result;
  result.success = false;

  try {
    LOG(INFO) << "=== Starting record simulation ===";

    if (!parser_.Parse(record_json_str)) {
      result.error_message = "Failed to parse record";
      LOG(ERROR) << result.error_message;
      return result;
    }

    LOG(INFO) << "Record parsed successfully";

    state_.Reset();
    step_logs_.clear();
    ProcessGameInfoAndSetup();
    LogGameInfo();
    ProcessAllActions();

    LOG(INFO) << "All actions processed, total steps: " << step_logs_.size();

    ExtractWinInfoFromScript();

    result.success = true;
    analyzer_.SetGameState(state_);
    analyzer_.SetScriptData(parser_.GetScriptData());
    result.win_analysis       = analyzer_.Analyze();
    result.game_log           = game_log_;
    result.game_log.step_logs = step_logs_;

    LOG(INFO) << "=== Simulation completed successfully ===";
    return result;
  } catch (const std::exception& e) {
    result.error_message = std::string("Simulation error: ") + e.what();
    LOG(ERROR) << result.error_message;
    return result;
  }
}

void RecordSimulator::ProcessGameInfoAndSetup() {
  LOG(INFO) << "Setting up game and dealing initial tiles";

  const auto& script_data = parser_.GetScriptData();

  if (!script_data.contains("w")) {
    LOG(ERROR) << "Wall data not found in script";
    return;
  }

  std::string wall_hex = script_data["w"].get<std::string>();
  std::vector<int> wall_indices;
  for (size_t i = 0; i < wall_hex.length(); i += 2) {
    std::string hex_pair = wall_hex.substr(i, 2);
    wall_indices.push_back(std::stoi(hex_pair, nullptr, 16));
  }

  LOG(INFO) << "Wall loaded with " << wall_indices.size() << " tiles";
  LOG(INFO) << "DEBUG: First 20 wall tiles: ";
  for (size_t i = 0; i < std::min(size_t(20), wall_indices.size()); ++i) {
    LOG(INFO) << "  wall[" << i << "] = " << wall_indices[i];
  }

  if (!script_data.contains("d")) {
    LOG(ERROR) << "Dice data not found in script";
    return;
  }

  int dice_val            = script_data["d"].get<int>();
  std::array<int, 4> dice = {
      dice_val & 15,
      (dice_val >> 4) & 15,
      (dice_val >> 8) & 15,
      (dice_val >> 12) & 15};

  LOG(INFO) << "Dice rolls: [" << dice[0] << ", " << dice[1] << ", " << dice[2]
            << ", " << dice[3] << "]";

  int dealer_idx = 0;
  state_.SetupWallAndDeal(wall_indices, dice, dealer_idx);

  LOG(INFO) << "Initial tiles dealt to all players";
  for (int i = 0; i < 4; ++i) {
    const auto& hand = state_.GetInitialHand(i);
    LOG(INFO) << "  Player " << base::WIND[i]
              << " initial hand size: " << hand.size();
  }
}

void RecordSimulator::LogGameInfo() {
  const auto& script_data = parser_.GetScriptData();
  const auto& game_config = parser_.GetGameConfig();
  const auto& player_info = parser_.GetPlayerInfo();

  if (game_config.contains("t")) {
    game_log_.game_title = game_config["t"].get<std::string>();
    LOG(INFO) << "Game title: " << game_log_.game_title;
  }

  game_log_.dealer_idx = state_.GetDealerIdx();
  if (player_info.is_array() && player_info.size() == 4) {
    for (size_t i = 0; i < 4; ++i) {
      std::string name = player_info[i]["n"].get<std::string>();
      game_log_.player_names.push_back(name);
      LOG(INFO) << base::WIND[i] << "家: " << name;
    }
  }
}

void RecordSimulator::ProcessAllActions() {
  LOG(INFO) << "Processing game actions";

  const auto& actions = parser_.GetActions();
  int prev_time       = 0;
  int step_number     = 0;
  size_t action_idx   = 0;

  for (const auto& action : actions) {
    int time_elapsed_ms = action.time_ms - prev_time;
    step_number++;

    std::string action_desc = BuildActionDescription(action);
    processor_.ProcessAction(action);
    LogAction(step_number, action, time_elapsed_ms, action_desc);

    int a_type = action.action_type;
    if (a_type == 6) {
      int winner_idx = action.player_idx;
      int fan_count  = action.data >> 1;

      bool is_self_drawn = false;
      if (action_idx > 0) {
        for (int i = action_idx - 1; i >= 0; --i) {
          const auto& prev_action = actions[i];
          int prev_type           = prev_action.action_type;

          if (prev_type == 8 || prev_type == 9) {
            continue;
          }

          if (prev_type == 7 && prev_action.player_idx == winner_idx) {
            is_self_drawn = true;
          }
          break;
        }
      }

      const auto& script_data = parser_.GetScriptData();
      if (script_data.contains("b")) {
        int win_flags        = script_data["b"].get<int>();
        int script_winner    = -1;
        int script_discarder = -1;

        for (int i = 0; i < 4; ++i) {
          if ((win_flags & (1 << i)) != 0) {
            script_winner = i;
          }
          if ((win_flags & (1 << (i + 4))) != 0) {
            script_discarder = i;
          }
        }

        bool script_is_self_drawn =
            (script_discarder < 0 || script_discarder == script_winner);

        if (script_winner >= 0 && script_winner == winner_idx) {
          if (is_self_drawn != script_is_self_drawn) {
            LOG(ERROR) << "ASSERTION FAILED: is_self_drawn mismatch!";
            LOG(ERROR) << "  Deduced from actions: "
                       << (is_self_drawn ? "true" : "false");
            LOG(ERROR) << "  From script data: "
                       << (script_is_self_drawn ? "true" : "false");
            LOG(ERROR) << "  Script discarder_idx: " << script_discarder;
            is_self_drawn = script_is_self_drawn;
          } else {
            LOG(INFO) << "is_self_drawn validation passed: "
                      << (is_self_drawn ? "SELF-DRAWN" : "OTHERS-WIN");
          }
        }
      }

      LOG(INFO) << "=== PLAYER HU ATTEMPT ===";
      LOG(INFO) << "Winner idx: " << winner_idx << " ("
                << game_log_.player_names[winner_idx] << ", "
                << base::WIND[winner_idx] << ") with " << fan_count
                << " fan(s)";
      LOG(INFO) << "Is self-drawn: "
                << (is_self_drawn ? "YES (自摸)" : "NO (点和)");

      if (fan_count == 0) {
        LOG(WARNING) << "ERROR HU (错和)! Game continues...";

        if (action_idx + 1 >= actions.size()) {
          LOG(WARNING) << "错和 but no more actions, game ends";
          break;
        }

      } else {
        LOG(INFO) << "Valid HU detected!";

        int win_tile = is_self_drawn ? state_.GetLastDrawTile(winner_idx)
                                     : state_.GetLastDiscardTile();

        LOG(INFO) << "Win tile value: " << win_tile;
        LOG(INFO) << "Win tile: " << utils::Tile::ToString(win_tile)
                  << " (self_drawn: " << (is_self_drawn ? "true" : "false")
                  << ")";

        const auto& hand = state_.GetPlayerHand(winner_idx);
        LOG(INFO) << "Winner's hand (" << hand.size() << " tiles):";
        for (int tile : hand) {
          LOG(INFO) << "  " << tile << " = " << utils::Tile::ToString(tile);
        }

        analyzer_.SetWinInfo(winner_idx, win_tile, is_self_drawn);

        if (action_idx + 1 >= actions.size()) {
          LOG(INFO) << "No more actions, game ends.";
          break;
        } else {
          LOG(INFO) << "More actions remaining, continue processing (possible "
                       "一炮多响)...";
        }
      }
    }

    prev_time = action.time_ms;
    action_idx++;
  }

  LOG(INFO) << "All actions processed, total steps recorded: "
            << step_logs_.size();
}

void RecordSimulator::ExtractWinInfoFromScript() {
  const auto& script_data = parser_.GetScriptData();

  if (!script_data.contains("y") || script_data["y"].empty()) {
    LOG(WARNING) << "No win info in script data";
    return;
  }

  auto win_flags = script_data.value("b", 0);

  if ((win_flags & 0x0F) == 0) {
    LOG(INFO) << "No valid winner in script data (荒庄)";
    return;
  }

  int winner_idx = -1;
  for (int i = 0; i < 4; ++i) {
    if ((win_flags & (1 << i)) != 0) {
      winner_idx = i;
      break;
    }
  }

  if (winner_idx < 0) {
    LOG(WARNING) << "Cannot determine winner from win flags: 0x" << std::hex
                 << win_flags;
    return;
  }

  int discarder_idx = -1;
  for (int i = 0; i < 4; ++i) {
    if ((win_flags & (1 << (i + 4))) != 0) {
      discarder_idx = i;
      break;
    }
  }

  bool is_self_drawn = (discarder_idx < 0 || discarder_idx == winner_idx);

  LOG(INFO) << "Extracting win info from script data:";
  LOG(INFO) << "  Winner: " << game_log_.player_names[winner_idx] << " ("
            << base::WIND[winner_idx] << ")";
  LOG(INFO) << "  Is self-drawn: " << (is_self_drawn ? "true" : "false");

  auto win_data = script_data["y"][winner_idx];
  if (win_data.contains("h")) {
    int win_tile = -1;
    if (is_self_drawn) {
      win_tile = state_.GetLastDrawTile(winner_idx);
    } else {
      win_tile = state_.GetLastDiscardTile();
    }

    if (win_tile >= 0) {
      LOG(INFO) << "  Win tile: " << utils::Tile::ToString(win_tile);
      analyzer_.SetWinInfo(winner_idx, win_tile, is_self_drawn);
    } else {
      LOG(WARNING) << "Cannot determine win tile";
    }
  }
}

void RecordSimulator::LogAction(
    int step_number,
    const Action& action,
    int time_elapsed_ms,
    const std::string& action_desc) {
  int p_idx               = action.player_idx;
  int a_type              = action.action_type;
  std::string player_name = game_log_.player_names[p_idx];
  std::string player_wind = base::WIND[p_idx];

  std::ostringstream log_msg;
  log_msg << "[Step " << step_number << "] " << player_wind << "家 "
          << player_name << " (+" << time_elapsed_ms / 1000.0 << "s) "
          << action_desc;

  LOG(INFO) << log_msg.str();

  if (a_type == 2 || a_type == 3 || a_type == 4 || a_type == 5 || a_type == 6 ||
      a_type == 7) {
    auto hand_str    = GetPlayerHandStrings(p_idx);
    auto pack_str    = GetPlayerPackStrings(p_idx);
    auto discard_str = GetPlayerDiscardStrings(p_idx);

    std::ostringstream state_msg;
    state_msg << "Hand: ";
    for (const auto& tile : hand_str) {
      state_msg << tile << " ";
    }
    state_msg << "| Packs: ";
    for (const auto& pack : pack_str) {
      state_msg << pack << " ";
    }
    state_msg << "| Discards: ";
    for (const auto& tile : discard_str) {
      state_msg << tile << " ";
    }

    LOG(INFO) << "  " << state_msg.str();
  }

  step_logs_.push_back(
      BuildStepLog(step_number, action, time_elapsed_ms, action_desc));
}

StepLog RecordSimulator::BuildStepLog(
    int step_number,
    const Action& action,
    int time_elapsed_ms,
    const std::string& action_desc) {
  StepLog log;
  log.step_number        = step_number;
  log.player_idx         = action.player_idx;
  log.player_name        = game_log_.player_names[action.player_idx];
  log.player_wind        = base::WIND[action.player_idx];
  log.action_type        = action.action_type;
  log.action_description = action_desc;
  log.time_elapsed_ms    = time_elapsed_ms;
  log.hand_tiles         = GetPlayerHandStrings(action.player_idx);
  log.pack_tiles         = GetPlayerPackStrings(action.player_idx);
  log.discard_tiles      = GetPlayerDiscardStrings(action.player_idx);

  return log;
}

std::string RecordSimulator::BuildActionDescription(const Action& action) {
  int p_idx  = action.player_idx;
  int a_type = action.action_type;
  int data   = action.data;

  int lo_byte = data & 0xFF;
  int hi_byte = (data >> 8) & 0xFF;

  std::ostringstream desc;

  switch (a_type) {
  case 0:
    desc << "开始出牌";
    break;
  case 1: {
    int ot       = (hi_byte & 15) + 136;
    bool is_auto = data & 0x1000;
    desc << (is_auto ? "自动" : "手动") << "补花 " << utils::Tile::ToString(ot)
         << " -> " << utils::Tile::ToString(lo_byte);
    break;
  }
  case 2: {
    int tile            = lo_byte;
    bool is_hand_played = hi_byte & 1;
    desc << (is_hand_played ? "手打" : "摸打") << " "
         << utils::Tile::ToString(tile);
    break;
  }
  case 3:
  case 4:
  case 5: {
    if (data == 0) {
      desc << "动作无效";
      break;
    }
    int tile_val            = (data & 0x3F) << 2;
    int actual_tile         = tile_val + ((data >> 10) & 3);
    int offer_direction     = (data >> 6) & 3;
    std::string action_name = base::PACK_ACTION_MAP.at(a_type);

    if (a_type == 3) {
      int last_discard = state_.GetLastDiscardTile();
      if (last_discard >= 0) {
        desc << action_name << " " << utils::Tile::ToString(last_discard);
      } else {
        desc << action_name << " " << utils::Tile::ToString(actual_tile);
      }
    } else {
      desc << action_name << " " << utils::Tile::ToString(actual_tile);
    }
    break;
  }
  case 6: {
    bool is_auto = data & 1;
    int fan      = data >> 1;
    desc << (is_auto ? "自动" : "手动") << "和";
    if (fan > 0) {
      desc << " " << fan << "番";
    }
    break;
  }
  case 7: {
    int tile_to_draw = lo_byte;
    bool is_reverse  = hi_byte != 0;
    desc << (is_reverse ? "逆向摸牌" : "摸牌") << " "
         << utils::Tile::ToString(tile_to_draw);
    break;
  }
  case 8:
    desc << "过";
    break;
  case 9:
    desc << "弃";
    break;
  default:
    desc << "未知动作(" << a_type << ")";
    break;
  }

  return desc.str();
}

std::vector<std::string>
RecordSimulator::GetPlayerHandStrings(int player_idx) const {
  std::vector<std::string> result;
  const auto& hand = state_.GetPlayerHand(player_idx);
  for (int tile : hand) {
    result.push_back(utils::Tile::ToString(tile));
  }
  return result;
}

std::vector<std::string>
RecordSimulator::GetPlayerPackStrings(int player_idx) const {
  std::vector<std::string> result;
  const auto& packs = state_.GetPlayerPacks(player_idx);
  for (const auto& pack : packs) {
    std::ostringstream pack_str;
    pack_str << "[";
    for (size_t i = 0; i < pack.size(); ++i) {
      pack_str << utils::Tile::ToString(pack[i]);
    }
    pack_str << "]";
    result.push_back(pack_str.str());
  }
  return result;
}

std::vector<std::string>
RecordSimulator::GetPlayerDiscardStrings(int player_idx) const {
  std::vector<std::string> result;
  const auto& discards = state_.GetPlayerDiscards(player_idx);
  for (int tile : discards) {
    result.push_back(utils::Tile::ToString(tile));
  }
  return result;
}

const GameLog& RecordSimulator::GetGameLog() const { return game_log_; }

const std::vector<StepLog>& RecordSimulator::GetStepLogs() const {
  return step_logs_;
}

} // namespace analyzer
} // namespace tziakcha
