#include "analyzer/win_analyzer.h"
#include "analyzer/game_state.h"
#include "base/mahjong_constants.h"
#include "utils/tile.h"
#include "utils/gb_format_converter.h"
#include "calc/fan_calculator.h"
#include <algorithm>
#include <glog/logging.h>

namespace tziakcha {
namespace analyzer {

WinAnalyzer::WinAnalyzer()
    : winner_idx_(-1), win_tile_(-1), is_self_drawn_(false), state_(nullptr) {}

void WinAnalyzer::SetWinInfo(int winner_idx, int win_tile, bool is_self_drawn) {
  winner_idx_    = winner_idx;
  win_tile_      = win_tile;
  is_self_drawn_ = is_self_drawn;
}

void WinAnalyzer::SetGameState(const GameState& state) { state_ = &state; }

void WinAnalyzer::SetScriptData(const json& script_data) {
  script_data_ = script_data;
}

WinAnalysis WinAnalyzer::Analyze() {
  WinAnalysis result;

  if (winner_idx_ < 0 || !state_) {
    return result;
  }

  result.winner_idx   = winner_idx_;
  result.winner_name  = script_data_["p"][winner_idx_]["n"].get<std::string>();
  result.winner_wind  = GetWindChar(winner_idx_);
  result.flower_count = state_->GetFlowerCount(winner_idx_);

  auto win_data    = script_data_["y"][winner_idx_];
  result.total_fan = win_data.value("f", 0);

  result.formatted_hand      = BuildFormattedHand();
  result.hand_string_for_gb  = BuildHandStringForGB();
  result.gb_handtiles_string = result.hand_string_for_gb;
  result.env_flag            = BuildEnvFlag();
  result.fan_details         = ExtractFanDetails();

  int calculated_fan_sum = 0;
  for (const auto& detail : result.fan_details) {
    calculated_fan_sum += detail.fan_points * detail.count;
  }
  result.base_fan = calculated_fan_sum;

  result.calculated_fan = CalculateFanWithGB(result.hand_string_for_gb);
  result.gb_fan_details = gb_fan_details_;

  LOG(INFO) << "Win Analysis for player " << winner_idx_ << " ("
            << result.winner_name << ")";
  LOG(INFO) << "  Total Fan from script: " << result.total_fan;
  LOG(INFO) << "  Base Fan from script: " << result.base_fan;
  LOG(INFO) << "  Calculated Fan from GB-Mahjong: " << result.calculated_fan;
  LOG(INFO) << "  GB Handtiles String: " << result.hand_string_for_gb;

  return result;
}

bool WinAnalyzer::IsLastCopyTile(int tile) const {
  if (!state_) {
    return false;
  }

  if (IsRobbingKong(is_self_drawn_)) {
    return false;
  }

  int base = tile >> 2;

  int exposed_melds = 0;
  int from_discards = 0;

  for (int p_idx = 0; p_idx < 4; ++p_idx) {
    const auto& packs     = state_->GetPlayerPacks(p_idx);
    const auto& pack_seqs = state_->GetPlayerPackOfferSequences(p_idx);

    for (size_t pack_idx = 0; pack_idx < packs.size(); ++pack_idx) {
      const auto& pack = packs[pack_idx];
      if (pack.empty())
        continue;
      int tiles_of_base = 0;
      int is_anko       = 0;
      for (int tile_in_pack : pack) {
        if ((tile_in_pack >> 2) == base) {
          tiles_of_base++;
          exposed_melds++;
          is_anko++;
        }
      }
      if (is_anko == 3) {
        LOG(INFO) << "  Found ankan of tile " << GetTileString(tile)
                  << " in player " << p_idx << "'s melds.";
        return true;
      }
      int num = pack_seqs[pack_idx];
      LOG(INFO) << p_idx << ", pack index " << pack_idx << ", seq " << num
                << ", pack " << GetTileString(pack[num]);
      if ((pack[num] >> 2) == base) {
        from_discards += 1;
      }
    }
  }

  int exposed_discards = 0;
  for (int p_idx = 0; p_idx < 4; ++p_idx) {
    const auto& discards = state_->GetPlayerDiscards(p_idx);
    for (int discard_tile : discards) {
      if ((discard_tile >> 2) == base) {
        exposed_discards++;
      }
    }
  }

  LOG(INFO) << "  Exposed melds of tile " << GetTileString(tile) << ": "
            << exposed_melds;
  LOG(INFO) << "  Exposed discards of tile " << GetTileString(tile) << ": "
            << exposed_discards;
  LOG(INFO) << "  Tiles in melds from discards: " << from_discards;

  int total_exposed = exposed_melds + exposed_discards;
  LOG(INFO) << "  Total exposed (adjusted): " << total_exposed;

  int required_count = is_self_drawn_ ? 3 : 4;
  if (total_exposed > required_count) {
    LOG(ERROR) << "  Last copy method error: More than " << required_count
               << " exposed tiles.";
  }
  return total_exposed >= required_count;
}

bool WinAnalyzer::IsSeaLastTile(bool is_self_drawn) const {
  if (!state_) {
    return false;
  }
  int front = state_->GetWallFrontPtr();
  int back  = state_->GetWallBackPtr();

  bool is_sea = (front > back);

  LOG(INFO) << "  IsSeaLastTile check: is_self_drawn=" << is_self_drawn
            << ", wall_front=" << front << ", wall_back=" << back
            << ", is_sea=" << (is_sea ? "true" : "false");

  return is_sea;
}

bool WinAnalyzer::IsRobbingKong(bool is_self_drawn) const {
  if (is_self_drawn) {
    return false;
  }
  if (!state_) {
    return false;
  }
  return state_->IsLastActionAddKong();
}

std::string WinAnalyzer::BuildEnvFlag() {
  if (winner_idx_ < 0 || !state_) {
    return "";
  }

  char round_wind = GetRoundWindChar()[0];
  char seat_wind  = GetWindChar(winner_idx_)[0];

  return utils::GBFormatConverter::BuildEnvFlag(
      round_wind,
      seat_wind,
      is_self_drawn_,
      IsLastCopyTile(win_tile_),
      IsSeaLastTile(is_self_drawn_),
      IsRobbingKong(is_self_drawn_));
}

std::string WinAnalyzer::BuildHandStringForGB() {
  if (winner_idx_ < 0 || !state_) {
    return "";
  }

  int w_idx                = winner_idx_;
  const auto& hand         = state_->GetPlayerHand(w_idx);
  const auto& packs        = state_->GetPlayerPacks(w_idx);
  const auto& pack_dirs    = state_->GetPlayerPackDirections(w_idx);
  const auto& pack_seqs    = state_->GetPlayerPackOfferSequences(w_idx);
  const auto& flower_tiles = state_->GetPlayerFlowerTiles(w_idx);

  std::vector<int> pack_directions_vec(packs.size(), 0);
  for (size_t i = 0; i < packs.size(); ++i) {
    pack_directions_vec[i] = pack_dirs[i];
  }

  char round_wind = GetRoundWindChar()[0];
  char seat_wind  = GetWindChar(winner_idx_)[0];

  std::vector<int> empty_flowers;

  return utils::GBFormatConverter::BuildFullGBString(
      hand,
      packs,
      pack_directions_vec,
      win_tile_,
      round_wind,
      seat_wind,
      is_self_drawn_,
      IsLastCopyTile(win_tile_),
      IsSeaLastTile(is_self_drawn_),
      IsRobbingKong(is_self_drawn_),
      0,
      empty_flowers);
}

std::string WinAnalyzer::BuildFormattedHand() {
  if (winner_idx_ < 0 || !state_) {
    return "";
  }

  int w_idx              = winner_idx_;
  std::vector<int> tiles = state_->GetPlayerHand(w_idx);
  std::sort(tiles.begin(), tiles.end());

  std::string result;
  for (int tile : tiles) {
    result += utils::Tile::ToString(tile);
    result += " ";
  }

  return result;
}

std::vector<FanDetail> WinAnalyzer::ExtractFanDetails() {
  std::vector<FanDetail> result;

  if (winner_idx_ < 0) {
    return result;
  }

  auto win_data = script_data_["y"][winner_idx_];
  if (!win_data.is_object()) {
    return result;
  }

  auto fan_details = win_data.value("t", json::object());
  if (!fan_details.is_object()) {
    return result;
  }

  for (auto& [fan_id_str, fan_val] : fan_details.items()) {
    int fan_id = std::stoi(fan_id_str);
    if (fan_id == 83) {
      continue;
    }

    int fan_val_int = fan_val.get<int>();
    int fan_points  = fan_val_int & 0xFF;
    int count       = ((fan_val_int >> 8) & 0xFF) + 1;
    std::string fan_name;

    if (fan_id >= 0 && fan_id < static_cast<int>(base::FAN_NAMES.size())) {
      fan_name = base::FAN_NAMES[fan_id];
    } else {
      fan_name = "Unknown(" + std::to_string(fan_id) + ")";
    }

    result.push_back({fan_id, fan_name, fan_points, count});
  }

  return result;
}

int WinAnalyzer::CountExposedTiles(int tile_base) const {
  if (!state_) {
    return 0;
  }

  int count = 0;
  for (int p_idx = 0; p_idx < 4; ++p_idx) {
    const auto& packs = state_->GetPlayerPacks(p_idx);
    for (const auto& pack : packs) {
      for (int tile : pack) {
        if ((tile >> 2) == tile_base) {
          count++;
        }
      }
    }

    const auto& discards = state_->GetPlayerDiscards(p_idx);
    for (int tile : discards) {
      if ((tile >> 2) == tile_base) {
        count++;
      }
    }
  }

  return count;
}

std::string WinAnalyzer::GetTileString(int index) const {
  return utils::Tile::ToString(index);
}

std::string WinAnalyzer::GetWindChar(int player_idx) const {
  const std::array<std::string, 4> wind_chars = {"E", "S", "W", "N"};
  return wind_chars[player_idx % 4];
}

std::string WinAnalyzer::GetRoundWindChar() const {
  auto gi_val = script_data_.value("i", json(0));
  if (gi_val.is_number_integer()) {
    int val                                     = gi_val.get<int>();
    const std::array<std::string, 4> wind_chars = {"E", "S", "W", "N"};
    return wind_chars[(val / 4) % 4];
  }
  return "E";
}

int WinAnalyzer::CalculateFanWithGB(const std::string& gb_string) {
  if (gb_string.empty()) {
    LOG(WARNING) << "Empty GB string, cannot calculate fan";
    return 0;
  }

  try {
    LOG(INFO) << "Calculating fan using GB-Mahjong library with string: "
              << gb_string;

    calc::FanCalculator calculator;

    if (!calculator.ParseHandtiles(gb_string)) {
      LOG(ERROR) << "Failed to parse handtiles string: " << gb_string;
      return 0;
    }

    if (!calculator.IsWinningHand()) {
      LOG(WARNING) << "Not a valid winning hand: " << gb_string;
      return 0;
    }

    if (!calculator.CalculateFan()) {
      LOG(ERROR) << "Failed to calculate fan for: " << gb_string;
      return 0;
    }

    int calculated_fan = calculator.GetTotalFan();
    LOG(INFO) << "Calculated fan: " << calculated_fan;

    gb_fan_details_.clear();
    auto fan_details = calculator.GetFanTypesSummary();
    if (!fan_details.empty()) {
      LOG(INFO) << "GB-Mahjong Fan type details:";
      for (const auto& detail : fan_details) {
        LOG(INFO) << "  - " << detail.fan_name << ": " << detail.count
                  << " pattern(s), " << detail.score_per_fan << " fan each, "
                  << "total: " << detail.total_score << " fan";

        gb_fan_details_.push_back(
            {detail.fan_name, detail.score_per_fan, detail.count});
      }
    }

    return calculated_fan;

  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception during fan calculation: " << e.what();
    return 0;
  }
}

} // namespace analyzer
} // namespace tziakcha
