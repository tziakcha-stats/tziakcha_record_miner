#include "stats/intercept_stats.h"
#include "analyzer/win_analyzer.h"
#include "base/mahjong_constants.h"
#include "utils/gb_format_converter.h"
#include "utils/tile.h"
#include <algorithm>
#include <glog/logging.h>
#include <sstream>

namespace tziakcha {
namespace stats {

std::string InterceptEvent::ToString() const {
  std::ostringstream ss;
  ss << "[Step " << step_number << "] ";

  if (!is_intercept || potential_winners.empty()) {
    ss << "点炮者: " << discarder_idx
       << " 打出: " << utils::Tile::ToString(discard_tile) << " | 无截和";
    return ss.str();
  }

  int winner     = potential_winners.front();
  int winner_fan = !potential_fans.empty() ? potential_fans.front() : 0;
  ss << winner << " " << winner_fan << " Fan intercept ";

  for (size_t i = 1; i < potential_winners.size(); ++i) {
    if (i > 1) {
      ss << " and ";
    }
    ss << potential_winners[i] << " " << potential_fans[i] << " Fan";
  }

  ss << " (discarder " << discarder_idx << " "
     << utils::Tile::ToString(discard_tile) << ")";
  return ss.str();
}

std::string InterceptStatsResult::ToSummary() const {
  std::ostringstream ss;
  ss << "=== 截和统计 ===\n";
  ss << "总点和次数: " << total_ron_wins << "\n";
  ss << "截和次数: " << intercept_count << "\n";
  ss << "截和率: " << (intercept_rate * 100.0) << "%\n";
  ss << "\n详细事件:\n";
  for (const auto& event : events) {
    if (event.is_intercept) {
      ss << event.ToString() << "\n\n";
    }
  }
  return ss.str();
}

InterceptStats::InterceptStats() {}

void InterceptStats::SetRoundId(const std::string& round_id) {
  current_round_id_ = round_id;
}

InterceptEvent InterceptStats::CheckIntercept(
    int discarder_idx,
    int discard_tile,
    const analyzer::GameState& game_state,
    int dealer_idx,
    int round_wind_index,
    int step_number) {
  InterceptEvent event;
  event.step_number   = step_number;
  event.discarder_idx = discarder_idx;
  event.discard_tile  = discard_tile;
  event.winner_idx    = -1;
  event.is_intercept  = false;

  std::vector<int> priority_order = GetWinPriorityOrder(discarder_idx);

  LOG(INFO) << "=== 检测截和 Step " << step_number << " ===";
  LOG(INFO) << "点炮者: " << discarder_idx << " (" << base::WIND[discarder_idx]
            << ") 打出: " << utils::Tile::ToString(discard_tile);
  LOG(INFO) << "检查顺序: ";
  for (int idx : priority_order) {
    LOG(INFO) << "  " << idx << " (" << base::WIND[idx] << ")";
  }

  for (int player_idx : priority_order) {
    int fan = CalculateWinFan(
        player_idx, discard_tile, game_state, dealer_idx, round_wind_index);

    if (fan >= 8) {
      event.potential_winners.push_back(player_idx);
      event.potential_fans.push_back(fan);

      LOG(INFO) << "  玩家 " << player_idx << " (" << base::WIND[player_idx]
                << ") 可以和牌! 番数: " << fan;

      if (event.winner_idx < 0) {
        event.winner_idx = player_idx;
      }
    }
  }

  if (event.potential_winners.size() > 1) {
    event.is_intercept = true;
    LOG(WARNING) << "*** 截和发生! ***";
    LOG(WARNING) << "  共 " << event.potential_winners.size() << " 人能和牌";
    LOG(WARNING) << "  实际和牌者: " << event.winner_idx << " ("
                 << base::WIND[event.winner_idx] << ")";
    LOG(WARNING) << "  被截和者: ";
    for (size_t i = 1; i < event.potential_winners.size(); ++i) {
      LOG(WARNING) << "    " << event.potential_winners[i] << " ("
                   << base::WIND[event.potential_winners[i]]
                   << ") 番数: " << event.potential_fans[i];
    }
  } else if (event.potential_winners.size() == 1) {
    LOG(INFO) << "  仅1人能和牌，无截和";
  } else {
    auto hand_state = [&](int idx) {
      std::ostringstream hs;
      hs << "P" << idx << " Hand: ";
      for (int t : game_state.GetPlayerHand(idx)) {
        hs << utils::Tile::ToString(t) << " ";
      }
      hs << "| Packs: ";
      const auto& packs = game_state.GetPlayerPacks(idx);
      for (const auto& p : packs) {
        hs << "[";
        for (int pt : p) {
          hs << utils::Tile::ToString(pt);
        }
        hs << "]";
      }
      return hs.str();
    };

    auto calc_str = [&](int idx) {
      static const char WIND_CHAR[4] = {'E', 'S', 'W', 'N'};
      char round_wind_char           = WIND_CHAR[round_wind_index % 4];
      return BuildHandtilesString(
          game_state.GetPlayerHand(idx),
          game_state.GetPlayerPacks(idx),
          game_state.GetPlayerPackDirections(idx),
          discard_tile,
          false,
          idx,
          dealer_idx,
          round_wind_char,
          game_state);
    };

    LOG(ERROR) << "  BUG? 无人能和牌 | round_id=" << current_round_id_
               << " | round_step=" << step_number << " discarder="
               << discarder_idx << " (" << base::WIND[discarder_idx]
               << ") tile=" << utils::Tile::ToString(discard_tile);
    for (int i = 0; i < 4; ++i) {
      LOG(ERROR) << "    " << hand_state(i);
      LOG(ERROR) << "    calc_string: " << calc_str(i);
    }
  }

  return event;
}

void InterceptStats::AddEvent(const InterceptEvent& event) {
  events_.push_back(event);
}

InterceptStatsResult InterceptStats::GetResult() const {
  InterceptStatsResult result{};
  result.events = events_;

  result.total_ron_wins  = 0;
  result.intercept_count = 0;

  for (const auto& event : events_) {
    if (!event.potential_winners.empty()) {
      result.total_ron_wins++;
      if (event.is_intercept) {
        result.intercept_count++;
      }
    }
  }

  if (result.total_ron_wins > 0) {
    result.intercept_rate =
        static_cast<double>(result.intercept_count) / result.total_ron_wins;
  } else {
    result.intercept_rate = 0.0;
  }

  return result;
}

void InterceptStats::Reset() { events_.clear(); }

int InterceptStats::CalculateWinFan(
    int player_idx,
    int win_tile,
    const analyzer::GameState& game_state,
    int dealer_idx,
    int round_wind_index) const {
  const auto& hand  = game_state.GetPlayerHand(player_idx);
  const auto& packs = game_state.GetPlayerPacks(player_idx);
  const auto& dirs  = game_state.GetPlayerPackDirections(player_idx);

  static const char WIND_CHAR[4] = {'E', 'S', 'W', 'N'};
  char round_wind_char           = WIND_CHAR[round_wind_index % 4];
  std::string handtiles_str      = BuildHandtilesString(
      hand,
      packs,
      dirs,
      win_tile,
      false,
      player_idx,
      dealer_idx,
      round_wind_char,
      game_state);

  LOG(INFO) << "    玩家 " << player_idx << " handtiles: " << handtiles_str;

  calc::FanCalculator calculator;
  if (!calculator.ParseHandtiles(handtiles_str)) {
    LOG(ERROR) << "    解析手牌失败";
    return 0;
  }

  if (!calculator.IsWinningHand()) {
    LOG(INFO) << "    不是和牌型";
    return 0;
  }

  if (!calculator.CalculateFan()) {
    LOG(ERROR) << "    计算番数失败";
    return 0;
  }

  int total_fan = calculator.GetTotalFan();
  return total_fan;
}

std::string InterceptStats::BuildHandtilesString(
    const std::vector<int>& hand,
    const std::vector<std::vector<int>>& packs,
    const std::vector<int>& pack_dirs,
    int win_tile,
    bool is_self_drawn,
    int player_idx,
    int dealer_idx,
    char round_wind_char,
    const analyzer::GameState& game_state) const {
  const auto& pack_seqs = game_state.GetPlayerPackOfferSequences(player_idx);
  std::vector<int> dirs_for_gb(packs.size(), 0);

  for (size_t i = 0; i < packs.size(); ++i) {
    dirs_for_gb[i] = pack_dirs[i];
    LOG(INFO) << "  Pack " << i
              << " seq: " << (i < pack_seqs.size() ? pack_seqs[i] : 0)
              << " -> dir: " << dirs_for_gb[i];
  }

  std::vector<int> hand_for_body = hand;
  if (is_self_drawn) {
    if (std::find(hand_for_body.begin(), hand_for_body.end(), win_tile) ==
        hand_for_body.end()) {
      hand_for_body.push_back(win_tile);
    }
  } else {
    auto it = std::find(hand_for_body.begin(), hand_for_body.end(), win_tile);
    if (it != hand_for_body.end()) {
      hand_for_body.erase(it);
    }
  }

  std::sort(hand_for_body.begin(), hand_for_body.end());

  std::string body = utils::GBFormatConverter::BuildCompleteHandString(
      hand_for_body, packs, dirs_for_gb, win_tile, is_self_drawn);

  analyzer::WinAnalyzer analyzer;
  analyzer.SetWinInfo(player_idx, win_tile, is_self_drawn);
  analyzer.SetGameState(game_state);

  bool is_last_copy    = analyzer.IsLastCopyTile(win_tile);
  bool is_sea_last     = analyzer.IsSeaLastTile(is_self_drawn);
  bool is_robbing_kong = analyzer.IsRobbingKong(is_self_drawn);

  static const char WIND_CHAR[4] = {'E', 'S', 'W', 'N'};
  body += "|";

  body.push_back(WIND_CHAR[(player_idx - dealer_idx + 4) % 4]);

  body.push_back(round_wind_char);
  body.push_back(is_self_drawn ? '1' : '0');
  body.push_back(is_last_copy ? '1' : '0');
  body.push_back(is_sea_last ? '1' : '0');
  body.push_back(is_robbing_kong ? '1' : '0');

  return body;
}

std::vector<int> InterceptStats::GetWinPriorityOrder(int discarder_idx) const {
  std::vector<int> order;

  for (int i = 1; i <= 3; ++i) {
    int player_idx = (discarder_idx + i) % 4;
    order.push_back(player_idx);
  }

  return order;
}

} // namespace stats
} // namespace tziakcha
