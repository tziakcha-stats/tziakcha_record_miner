#include "stats/player_stats.h"
#include "base/tziakcha.h"
#include "utils/thread_pool.h"

#include "base/mahjong_constants.h"
#include "stats/player_stats_config.h"
#include "storage/filesystem_storage.h"
#include "utils/script_decoder.h"
#include "analyzer/simulator.h"
#include "calc/shanten_calculator.h"
#include "utils/gb_format_converter.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <glog/logging.h>
#include <iomanip>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace tziakcha {
namespace stats {

struct EloPoint {
  std::string session_id;
  int64_t timestamp_ms = 0;
  double value         = 1500.0;
};

struct FanSummary {
  std::string name;
  int points = 0;
  int count  = 0;
};

struct WinEntry {
  std::string record_id;
  std::string session_id;
  int64_t timestamp_ms = 0;
  std::string win_type;
  int total_fan = 0;
  std::string hand_raw;
  std::string starting_hand_raw;
  std::vector<FanSummary> max_fans;
};

struct PlayerStats {
  std::string player_id;
  std::string name;
  std::unordered_set<std::string> aliases; // Track all names used
  double current_elo  = 1500.0;
  int64_t last_elo_ts = 0;

  int total_rounds             = 0;
  int win_count                = 0;
  int ron_win_count            = 0;
  int tsumo_win_count          = 0;
  int deal_in_count            = 0;
  int tsumo_against_count      = 0;
  int draw_count               = 0;
  double total_session_seconds = 0.0;
  int sessions_recorded        = 0;
  int64_t total_steps          = 0;

  // 13 tiles for dealing w/o next draw (or non-dealer start)
  int64_t total_starting_shanten_13 = 0;
  int starting_shanten_count_13     = 0;

  // 14 tiles for dealer start
  int64_t total_starting_shanten_14 = 0;
  int starting_shanten_count_14     = 0;

  std::vector<EloPoint> elo_history;
  std::vector<std::string> processed_sessions;
  std::vector<WinEntry> wins;

  std::unordered_set<std::string> processed_session_set;
};

struct PlayerSlot {
  int seat_idx;
  std::string id;
  std::string name;
};

struct RecordMeta {
  fs::path path;
  std::string record_id;
  std::string session_id;
  int64_t timestamp_ms = 0;
  json script_json;
  std::string content;
  json raw_json;
};

struct WinFlagInfo {
  std::vector<int> winners;
  int discarder = -1;
};

struct SessionInfo {
  int64_t min_ts      = std::numeric_limits<int64_t>::max();
  int64_t max_ts      = std::numeric_limits<int64_t>::min();
  int64_t duration_ms = 0;
  std::unordered_set<std::string> participants;
};

namespace {

bool ReadFile(const fs::path& path, std::string& out) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return false;
  }
  std::ostringstream ss;
  ss << ifs.rdbuf();
  out = ss.str();
  return true;
}

json ParseJson(const std::string& content, const fs::path& path) {
  try {
    return json::parse(content);
  } catch (const std::exception& e) {
    LOG(WARNING) << "Failed to parse json from " << path << ": " << e.what();
    return json::object();
  }
}

bool DecodeScript(const json& record_json, json& script_json) {
  if (!record_json.contains("script")) {
    return false;
  }
  std::string encoded = record_json.value("script", "");
  return utils::DecodeScriptToJson(encoded, script_json);
}

std::vector<PlayerSlot> ExtractPlayers(const json& script_json) {
  std::vector<PlayerSlot> players;
  if (!script_json.contains(PlayerStatsConfig::kScriptPlayers) ||
      !script_json[PlayerStatsConfig::kScriptPlayers].is_array()) {
    return players;
  }
  const auto& p_arr = script_json[PlayerStatsConfig::kScriptPlayers];
  for (size_t i = 0; i < p_arr.size(); ++i) {
    PlayerSlot slot{static_cast<int>(i), "", ""};
    const auto& obj = p_arr[i];
    if (obj.is_object()) {
      slot.id   = obj.value("i", "");
      slot.name = obj.value("n", "");
    }
    players.push_back(slot);
  }
  return players;
}

WinFlagInfo ParseWinFlags(const json& script_json) {
  WinFlagInfo info;
  int win_flags = script_json.value(PlayerStatsConfig::kScriptWinFlags, 0);

  for (int i = 0; i < 4; ++i) {
    if ((win_flags & (1 << i)) != 0) {
      info.winners.push_back(i);
    }
  }

  for (int i = 0; i < 4; ++i) {
    if ((win_flags & (1 << (i + 4))) != 0) {
      info.discarder = i;
      break;
    }
  }

  return info;
}

std::vector<FanSummary> ExtractMaxFans(const json& win_data) {
  std::vector<FanSummary> fans;
  if (!win_data.is_object() ||
      !win_data.contains(PlayerStatsConfig::kWinFanMap)) {
    return fans;
  }

  const auto& t_obj = win_data[PlayerStatsConfig::kWinFanMap];
  if (!t_obj.is_object()) {
    return fans;
  }

  int max_points = 0;
  struct FanDetailParsed {
    int fan_id;
    std::string name;
    int points;
    int count;
  };
  std::vector<FanDetailParsed> parsed;

  for (auto it = t_obj.begin(); it != t_obj.end(); ++it) {
    int fan_id  = std::stoi(it.key());
    int raw_val = it.value().get<int>();
    int points  = raw_val & 0xFF;
    int count   = ((raw_val >> 8) & 0xFF) + 1;
    std::string name =
        (fan_id >= 0 && fan_id < static_cast<int>(base::FAN_NAMES.size()))
            ? base::FAN_NAMES[fan_id]
            : ("Unknown(" + std::to_string(fan_id) + ")");

    parsed.push_back({fan_id, name, points, count});
    if (points > max_points) {
      max_points = points;
    }
  }

  for (const auto& p : parsed) {
    if (p.points >= 24 || p.points == max_points) {
      fans.push_back({p.name, p.points, p.count});
    }
  }
  return fans;
}

int64_t GetRecordTimestamp(const json& record_json) {
  if (record_json.contains(PlayerStatsConfig::kStep) &&
      record_json[PlayerStatsConfig::kStep].is_object()) {
    const auto& step_obj = record_json[PlayerStatsConfig::kStep];
    return step_obj.value(
        PlayerStatsConfig::kStepTimestamp,
        record_json.value(PlayerStatsConfig::kRecordTimestamp, 0LL));
  }
  return record_json.value(PlayerStatsConfig::kRecordTimestamp, 0LL);
}

struct DurationBreakdown {
  std::array<int64_t, 4> player_ms{};
  int64_t record_ms = 0;
};

DurationBreakdown ComputeActionDurations(const json& record_json) {
  DurationBreakdown out;
  out.player_ms.fill(0);

  if (!record_json.contains(PlayerStatsConfig::kStep) ||
      !record_json[PlayerStatsConfig::kStep].is_object()) {
    return out;
  }
  const auto& step_obj = record_json[PlayerStatsConfig::kStep];
  if (!step_obj.contains(PlayerStatsConfig::kStepActions) ||
      !step_obj[PlayerStatsConfig::kStepActions].is_array()) {
    return out;
  }

  const auto& acts = step_obj[PlayerStatsConfig::kStepActions];
  int64_t prev_t   = 0;

  for (const auto& act : acts) {
    if (!act.is_array() || act.size() <= PlayerStatsConfig::kActionTimeIndex ||
        !act[0].is_number_integer() ||
        !act[PlayerStatsConfig::kActionTimeIndex].is_number_integer()) {
      continue;
    }
    int64_t t = act[PlayerStatsConfig::kActionTimeIndex].get<int64_t>();
    if (t < 0) {
      LOG(WARNING) << "Skipping negative action time " << t;
      continue;
    }

    int64_t delta = t - prev_t;
    if (delta < 0) {
      LOG(WARNING) << "Non-monotonic action time encountered: prev_t=" << prev_t
                   << ", t=" << t << "; skipping delta";
      prev_t = t;
      continue;
    }

    int combined    = act[0].get<int>();
    int player_idx  = (combined >> 4) & 3;
    int action_type = combined & base::kActionTypeMask;
    int data =
        act.size() > 1 && act[1].is_number_integer() ? act[1].get<int>() : 0;

    bool is_auto_action = false;

    switch (action_type) {
    case base::kActionTypeFlowerReplace:
      is_auto_action = (data & base::kFlowerAutoMask) != 0;
      break;
    case base::kActionTypeWin:
      is_auto_action = (data & base::kWinAutoMask) != 0;
      break;
    case base::kActionTypeDraw:
      is_auto_action = true;
      break;
    case base::kActionTypePass:
      is_auto_action = (data & base::kPassModeMask) != 0;
      break;
    default:
      is_auto_action = false;
      break;
    }

    if (!is_auto_action && player_idx >= 0 &&
        player_idx < static_cast<int>(out.player_ms.size())) {
      out.player_ms[player_idx] += delta;
    }

    prev_t        = t;
    out.record_ms = t;
  }

  return out;
}

std::array<int64_t, 4> CountStepsByPlayer(const json& record_json) {
  std::array<int64_t, 4> counts{};
  counts.fill(0);

  if (!record_json.contains(PlayerStatsConfig::kStep) ||
      !record_json[PlayerStatsConfig::kStep].is_object()) {
    return counts;
  }
  const auto& step_obj = record_json[PlayerStatsConfig::kStep];
  if (!step_obj.contains(PlayerStatsConfig::kStepActions) ||
      !step_obj[PlayerStatsConfig::kStepActions].is_array()) {
    return counts;
  }

  const auto& acts = step_obj[PlayerStatsConfig::kStepActions];
  for (const auto& act : acts) {
    if (!act.is_array() || act.empty() || !act[0].is_number_integer()) {
      continue;
    }
    int combined   = act[0].get<int>();
    int player_idx = (combined >> 4) & 3;
    if (player_idx >= 0 && player_idx < static_cast<int>(counts.size())) {
      counts[player_idx]++;
    }
  }

  return counts;
}

json ToJson(const FanSummary& f) {
  return json{{"name", f.name}, {"points", f.points}, {"count", f.count}};
}

json ToJson(const WinEntry& w) {
  json obj;
  obj["record_id"]         = w.record_id;
  obj["session_id"]        = w.session_id;
  obj["timestamp_ms"]      = w.timestamp_ms;
  obj["win_type"]          = w.win_type;
  obj["total_fan"]         = w.total_fan;
  obj["hand_raw"]          = w.hand_raw;
  obj["starting_hand_raw"] = w.starting_hand_raw;
  obj["max_fans"]          = json::array();
  for (const auto& f : w.max_fans) {
    obj["max_fans"].push_back(ToJson(f));
  }
  return obj;
}

json ToJson(const EloPoint& e) {
  return json{{"session_id", e.session_id},
              {"timestamp_ms", e.timestamp_ms},
              {"elo", e.value}};
}

PlayerStats FromJson(const json& j) {
  PlayerStats ps;
  ps.player_id   = j.value("player_id", "");
  ps.name        = j.value("name", "");
  ps.current_elo = j.value("current_elo", 1500.0);
  ps.last_elo_ts = j.value("last_elo_ts", 0LL);

  const auto& stats_obj    = j.value("stats", json::object());
  ps.total_rounds          = stats_obj.value("total_rounds", 0);
  ps.win_count             = stats_obj.value("win_count", 0);
  ps.ron_win_count         = stats_obj.value("ron_win_count", 0);
  ps.tsumo_win_count       = stats_obj.value("tsumo_win_count", 0);
  ps.deal_in_count         = stats_obj.value("deal_in_count", 0);
  ps.tsumo_against_count   = stats_obj.value("tsumo_against_count", 0);
  ps.draw_count            = stats_obj.value("draw_count", 0);
  ps.total_session_seconds = stats_obj.value("total_session_seconds", 0.0);
  if (ps.total_session_seconds <= 0.0 &&
      stats_obj.contains("total_session_ms")) {
    ps.total_session_seconds =
        static_cast<double>(stats_obj.value("total_session_ms", 0LL)) / 1000.0;
  }
  ps.sessions_recorded = stats_obj.value("sessions_recorded", 0);
  ps.total_steps       = stats_obj.value("total_steps", 0);
  ps.total_starting_shanten_13 =
      stats_obj.value("total_starting_shanten_13", 0LL);
  ps.starting_shanten_count_13 =
      stats_obj.value("starting_shanten_count_13", 0);
  ps.total_starting_shanten_14 =
      stats_obj.value("total_starting_shanten_14", 0LL);
  ps.starting_shanten_count_14 =
      stats_obj.value("starting_shanten_count_14", 0);

  if (j.contains("aliases") && j["aliases"].is_array()) {
    for (const auto& a : j["aliases"]) {
      if (a.is_string()) {
        ps.aliases.insert(a.get<std::string>());
      }
    }
  }

  if (j.contains("elo_history") && j["elo_history"].is_array()) {
    for (const auto& item : j["elo_history"]) {
      EloPoint ep;
      ep.session_id   = item.value("session_id", "");
      ep.timestamp_ms = item.value("timestamp_ms", 0LL);
      ep.value        = item.value("elo", 1500.0);
      ps.elo_history.push_back(ep);
    }
  }

  if (j.contains("processed_sessions") && j["processed_sessions"].is_array()) {
    for (const auto& s : j["processed_sessions"]) {
      if (s.is_string()) {
        auto val = s.get<std::string>();
        ps.processed_sessions.push_back(val);
        ps.processed_session_set.insert(val);
      }
    }
  }

  if (j.contains("wins") && j["wins"].is_array()) {
    for (const auto& w : j["wins"]) {
      WinEntry we;
      we.record_id         = w.value("record_id", "");
      we.session_id        = w.value("session_id", "");
      we.timestamp_ms      = w.value("timestamp_ms", 0LL);
      we.win_type          = w.value("win_type", "");
      we.total_fan         = w.value("total_fan", 0);
      we.hand_raw          = w.value("hand_raw", "");
      we.starting_hand_raw = w.value("starting_hand_raw", "");

      if (w.contains("max_fans") && w["max_fans"].is_array()) {
        for (const auto& f : w["max_fans"]) {
          FanSummary fs;
          fs.name   = f.value("name", "");
          fs.points = f.value("points", 0);
          fs.count  = f.value("count", 0);
          we.max_fans.push_back(fs);
        }
      }
      ps.wins.push_back(we);
    }
  }

  return ps;
}

struct RecordProcessingResult {
  bool success = false;
  fs::path path;
  std::string record_id;
  std::string session_id;
  int64_t timestamp_ms = 0;

  // Extracted Data
  std::vector<PlayerSlot> slots;
  std::map<int, double> player_elos;
  DurationBreakdown durations;
  std::array<int64_t, 4> step_counts;

  // Win/Game handling
  bool is_draw       = false;
  bool is_self_drawn = false;
  int winner_idx     = -1;
  int discarder_idx  = -1;
  int total_fan      = 0;

  std::string hand_raw;
  std::string starting_hand_raw;
  std::vector<FanSummary> max_fans;

  // Shanten
  std::map<int, std::pair<int, int>>
      starting_shantens; // seat_idx -> {shanten, tile_count}
};

RecordProcessingResult ProcessRecord(const RecordMeta& meta) {
  RecordProcessingResult result;
  result.path         = meta.path;
  result.record_id    = meta.record_id;
  result.session_id   = meta.session_id;
  result.timestamp_ms = meta.timestamp_ms;

  std::string content;
  if (!meta.content.empty()) {
    content = meta.content;
  } else {
    // Reload if not present (should be reloaded in threaded context)
    if (!ReadFile(meta.path, content)) {
      LOG(WARNING) << "Failed to read record in worker: " << meta.path;
      return result;
    }
  }

  json raw_json = meta.raw_json;
  if (raw_json.is_null()) {
    raw_json = ParseJson(content, meta.path);
  }

  json script_json = meta.script_json;
  if (script_json.is_null()) {
    if (raw_json.contains("step") && raw_json["step"].is_object()) {
      script_json = raw_json["step"];
    } else if (!DecodeScript(raw_json, script_json)) {
      LOG(WARNING) << "Failed to decode script in worker: " << meta.path;
      return result;
    }
  }

  result.slots = ExtractPlayers(script_json);
  if (result.slots.empty()) {
    return result;
  }

  // Extract ELOs
  const json* player_e_src = nullptr;
  if (raw_json.contains(PlayerStatsConfig::kStep) &&
      raw_json[PlayerStatsConfig::kStep].is_object()) {
    const auto& step_obj = raw_json[PlayerStatsConfig::kStep];
    if (step_obj.contains(PlayerStatsConfig::kStepPlayers) &&
        step_obj[PlayerStatsConfig::kStepPlayers].is_array()) {
      player_e_src = &step_obj[PlayerStatsConfig::kStepPlayers];
    }
  } else if (script_json.contains(PlayerStatsConfig::kScriptPlayers) &&
             script_json[PlayerStatsConfig::kScriptPlayers].is_array()) {
    player_e_src = &script_json[PlayerStatsConfig::kScriptPlayers];
  }

  if (player_e_src) {
    const auto& sp = *player_e_src;
    for (size_t i = 0; i < sp.size() && i < result.slots.size(); ++i) {
      if (!sp[i].is_object())
        continue;
      double elo_val        = sp[i].value(PlayerStatsConfig::kEloField, 1500.0);
      result.player_elos[i] = elo_val;
    }
  }

  result.durations   = ComputeActionDurations(raw_json);
  result.step_counts = CountStepsByPlayer(raw_json);

  const auto flag_info = ParseWinFlags(script_json);
  result.winner_idx =
      flag_info.winners.empty() ? -1 : flag_info.winners.front();
  result.discarder_idx = flag_info.discarder;
  result.is_draw       = result.winner_idx < 0;
  result.is_self_drawn =
      (!result.is_draw &&
       (result.discarder_idx < 0 || result.discarder_idx == result.winner_idx));

  if (!result.is_draw && script_json.contains(PlayerStatsConfig::kScriptWins) &&
      script_json[PlayerStatsConfig::kScriptWins].is_array() &&
      result.winner_idx <
          static_cast<int>(
              script_json[PlayerStatsConfig::kScriptWins].size())) {
    const auto& win_data =
        script_json[PlayerStatsConfig::kScriptWins][result.winner_idx];

    result.total_fan =
        win_data.is_object()
            ? win_data.value(PlayerStatsConfig::kWinFanTotal, 0)
            : 0;
    result.max_fans = ExtractMaxFans(win_data);

    if (win_data.contains(PlayerStatsConfig::kWinHand)) {
      try {
        result.hand_raw = win_data[PlayerStatsConfig::kWinHand].dump();
      } catch (...) {
      }
    }
  }

  // Simulation
  analyzer::RecordSimulator simulator;
  auto sim_result = simulator.Simulate(content);

  if (sim_result.success) {
    calc::ShantenCalculator shanten_calc;

    // Determine round wind
    char round_wind = 'E'; // Default
    if (script_json.contains("i") && script_json["i"].is_number_integer()) {
      int g_i            = script_json["i"].get<int>();
      int r_idx          = (g_i / 4) % 4;
      const char winds[] = {'E', 'S', 'W', 'N'};
      if (r_idx >= 0 && r_idx < 4)
        round_wind = winds[r_idx];
    }

    // Calculate shantens
    for (size_t i = 0; i < result.slots.size(); ++i) {
      if (sim_result.starting_hands.count(i)) {
        const auto& raw_hand = sim_result.starting_hands[i];
        mahjong::Handtiles hand_tiles;
        for (int t : raw_hand) {
          int tile_id = (t >> 2) + 1;
          if (tile_id > 0 && tile_id <= 34) {
            hand_tiles.lipai.push_back(mahjong::Tile(tile_id));
            hand_tiles.lipai_table[tile_id]++;
          }
        }
        auto shanten_res            = shanten_calc.Calculate(hand_tiles);
        result.starting_shantens[i] = {
            shanten_res.standard, (int)raw_hand.size()};
      }
    }

    if (!result.is_draw &&
        sim_result.win_analysis.winner_idx == result.winner_idx) {
      result.hand_raw = sim_result.win_analysis.hand_string_for_gb;
    }

    if (!result.is_draw && sim_result.starting_hands.count(result.winner_idx)) {
      int w_idx            = result.winner_idx;
      const auto& raw_hand = sim_result.starting_hands[w_idx];

      // Convert to logic tiles for formatter
      std::vector<int> logic_hand =
          raw_hand; // raw_hand is already in valid range for formatter?
      // Simulator returns raw integers (0-135). GBFormatConverter expects raw
      // integers.

      const char winds[] = {'E', 'S', 'W', 'N'};
      char seat_wind     = winds[w_idx % 4];

      std::vector<std::vector<int>> empty_packs;
      std::vector<int> empty_dirs;
      std::vector<int> empty_flowers;

      result.starting_hand_raw = utils::GBFormatConverter::BuildFullGBString(
          logic_hand,
          empty_packs,
          empty_dirs,
          -1, // win_tile
          round_wind,
          seat_wind,
          false, // self_drawn
          false, // last_copy
          false, // sea_last
          false, // rob_kong
          0,     // flower info
          empty_flowers);
    }
  }

  result.success = true;
  return result;
}

json ToJson(const PlayerStats& ps) {
  json j;
  j["player_id"]   = ps.player_id;
  j["name"]        = ps.name;
  j["current_elo"] = ps.current_elo;
  j["last_elo_ts"] = ps.last_elo_ts;

  j["aliases"] = json::array();
  for (const auto& a : ps.aliases) {
    j["aliases"].push_back(a);
  }

  json stats_obj;
  stats_obj["total_rounds"]          = ps.total_rounds;
  stats_obj["win_count"]             = ps.win_count;
  stats_obj["ron_win_count"]         = ps.ron_win_count;
  stats_obj["tsumo_win_count"]       = ps.tsumo_win_count;
  stats_obj["deal_in_count"]         = ps.deal_in_count;
  stats_obj["tsumo_against_count"]   = ps.tsumo_against_count;
  stats_obj["draw_count"]            = ps.draw_count;
  stats_obj["total_session_seconds"] = ps.total_session_seconds;
  stats_obj["sessions_recorded"]     = ps.sessions_recorded;
  stats_obj["total_steps"]           = ps.total_steps;
  stats_obj["average_step_seconds"] =
      ps.total_steps > 0
          ? ps.total_session_seconds / static_cast<double>(ps.total_steps)
          : 0.0;

  stats_obj["avg_starting_shanten_13"] =
      ps.starting_shanten_count_13 > 0
          ? static_cast<double>(ps.total_starting_shanten_13) /
                static_cast<double>(ps.starting_shanten_count_13)
          : 0.0;
  stats_obj["total_starting_shanten_13"] = ps.total_starting_shanten_13;
  stats_obj["starting_shanten_count_13"] = ps.starting_shanten_count_13;

  stats_obj["avg_starting_shanten_14"] =
      ps.starting_shanten_count_14 > 0
          ? static_cast<double>(ps.total_starting_shanten_14) /
                static_cast<double>(ps.starting_shanten_count_14)
          : 0.0;
  stats_obj["total_starting_shanten_14"] = ps.total_starting_shanten_14;
  stats_obj["starting_shanten_count_14"] = ps.starting_shanten_count_14;

  j["stats"] = stats_obj;

  j["elo_history"] = json::array();
  for (const auto& ep : ps.elo_history) {
    j["elo_history"].push_back(ToJson(ep));
  }

  j["processed_sessions"] = ps.processed_sessions;

  j["wins"] = json::array();
  for (const auto& w : ps.wins) {
    j["wins"].push_back(ToJson(w));
  }

  return j;
}

} // namespace

bool RunPlayerStats(const PlayerStatsOptions& options) {
  fs::path record_dir = options.record_dir;
  if (!fs::exists(record_dir) || !fs::is_directory(record_dir)) {
    LOG(ERROR) << "Record directory not found: " << record_dir;
    return false;
  }

  storage::FileSystemStorage storage(options.output_dir);

  // 1. ThreadPool init
  unsigned int n_threads = std::thread::hardware_concurrency();
  if (n_threads == 0)
    n_threads = 4;
  utils::ThreadPool pool(n_threads);
  LOG(INFO) << "Starting PlayerStats with " << n_threads << " threads.";

  // 2. Scan Phase (Parallel Header Read)
  std::vector<std::future<RecordMeta>> scan_futures;
  int scan_limit_counter = 0;

  for (auto it = fs::recursive_directory_iterator(record_dir);
       it != fs::recursive_directory_iterator();
       ++it) {
    if (!it->is_regular_file() || it->path().extension() != ".json") {
      continue;
    }
    if (options.limit > 0 && scan_limit_counter >= options.limit) {
      break;
    }
    scan_limit_counter++;

    fs::path p = it->path();
    scan_futures.emplace_back(pool.enqueue([p] {
      RecordMeta meta;
      meta.path = p;
      std::string content;
      if (!ReadFile(p, content)) {
        return meta; // empty/failed
      }
      // Minimal parse to get timestamps/id
      auto record_json = ParseJson(content, p);
      if (record_json.is_discarded() || record_json.empty()) {
        return meta;
      }

      meta.record_id =
          record_json.value(PlayerStatsConfig::kRecordId, p.stem().string());
      meta.session_id   = record_json.value(PlayerStatsConfig::kSessionId, "");
      meta.timestamp_ms = GetRecordTimestamp(record_json);
      // Don't keep content/json in memory to save RAM
      return meta;
    }));
  }

  std::vector<RecordMeta> records;
  records.reserve(scan_futures.size());
  for (auto& f : scan_futures) {
    RecordMeta m = f.get();
    if (!m.record_id.empty()) { // Valid result
      records.push_back(std::move(m));
    }
  }

  // 3. Sort Phase
  std::sort(records.begin(),
            records.end(),
            [](const RecordMeta& a, const RecordMeta& b) {
              return a.timestamp_ms < b.timestamp_ms;
            });

  // 4. Process Phase (Parallel Simulation)
  std::vector<std::future<RecordProcessingResult>> process_futures;
  process_futures.reserve(records.size());
  for (const auto& r : records) {
    // Pass by copy/dispatch
    process_futures.emplace_back(
        pool.enqueue([r] { return ProcessRecord(r); }));
  }

  // 5. Merge Phase (Sequential Aggregation)
  std::unordered_map<std::string, PlayerStats> players;
  std::unordered_map<std::string, SessionInfo> sessions;

  // Helper to load/create player
  auto get_player = [&](const PlayerSlot& slot) -> PlayerStats& {
    auto it = players.find(slot.id);
    if (it != players.end()) {
      if (!slot.name.empty()) {
        it->second.name = slot.name;
      }
      return it->second;
    }
    // Load from storage or create new
    PlayerStats ps;
    ps.player_id = slot.id;
    ps.name      = slot.name;
    json existing;
    if (storage.load_json(slot.id, existing)) {
      ps = FromJson(existing);
      if (!slot.name.empty())
        ps.name = slot.name;
      // Negative time fix
      if (ps.total_session_seconds < 0.0) {
        PlayerStats reset;
        reset.player_id = slot.id;
        reset.name      = ps.name;
        ps              = std::move(reset);
      }
    }
    auto [iter, _] = players.emplace(slot.id, std::move(ps));
    return iter->second;
  };

  int processed_count = 0;
  for (auto& f : process_futures) {
    RecordProcessingResult res = f.get();
    if (!res.success) {
      continue;
    }

    processed_count++;
    if (processed_count % 100 == 0) {
      LOG(INFO) << "Processed " << processed_count << " records...";
    }

    auto& session  = sessions[res.session_id];
    session.min_ts = std::min(session.min_ts, res.timestamp_ms);
    session.max_ts = std::max(session.max_ts, res.timestamp_ms);
    session.duration_ms += res.durations.record_ms;

    // Apply ELOs
    for (const auto& [i, elo] : res.player_elos) {
      if (i < (int)res.slots.size()) {
        auto& ps = get_player(res.slots[i]);
        if (res.timestamp_ms >= ps.last_elo_ts) {
          ps.current_elo = elo;
          ps.last_elo_ts = res.timestamp_ms;
        }
      }
    }

    // Ensure players exist
    for (const auto& slot : res.slots)
      get_player(slot);

    std::vector<PlayerStats*> stats_ptrs;
    bool any_processed = false;
    bool all_processed = true;
    for (const auto& slot : res.slots) {
      PlayerStats& ps = get_player(slot);
      stats_ptrs.push_back(&ps);
      if (ps.processed_session_set.count(res.session_id)) {
        any_processed = true;
      } else {
        all_processed = false;
      }
    }

    if (any_processed) {
      continue;
    }

    // Update Stats
    for (size_t i = 0; i < res.slots.size(); ++i) {
      auto& ps = *stats_ptrs[i];
      if (!res.slots[i].name.empty()) {
        ps.aliases.insert(res.slots[i].name);
      }

      ps.total_rounds++;
      // Removed processed_records insertion

      int64_t p_steps =
          ((int)i < (int)res.step_counts.size()) ? res.step_counts[i] : 0;
      ps.total_steps += p_steps;

      double p_sec = ((int)i < (int)res.durations.player_ms.size())
                       ? (double)res.durations.player_ms[i] / 1000.0
                       : 0.0;
      ps.total_session_seconds += p_sec;
      session.participants.insert(ps.player_id);

      // Shanten
      if (res.starting_shantens.count(i)) {
        auto [val, count] = res.starting_shantens.at(i);
        if (count == 13) {
          ps.total_starting_shanten_13 += val;
          ps.starting_shanten_count_13++;
        } else if (count == 14) {
          ps.total_starting_shanten_14 += val;
          ps.starting_shanten_count_14++;
        }
      }

      if (res.is_draw) {
        ps.draw_count++;
        continue;
      }

      if ((int)i == res.winner_idx) {
        ps.win_count++;
        if (res.is_self_drawn)
          ps.tsumo_win_count++;
        else
          ps.ron_win_count++;

        WinEntry we;
        we.record_id         = res.record_id;
        we.session_id        = res.session_id;
        we.timestamp_ms      = res.timestamp_ms;
        we.win_type          = res.is_self_drawn ? "tsumo" : "ron";
        we.total_fan         = res.total_fan;
        we.hand_raw          = res.hand_raw;
        we.starting_hand_raw = res.starting_hand_raw;
        we.max_fans          = res.max_fans;
        ps.wins.push_back(std::move(we));
      } else {
        if (!res.is_self_drawn && res.discarder_idx == (int)i) {
          ps.deal_in_count++;
        }
        if (res.is_self_drawn) {
          ps.tsumo_against_count++;
        }
      }
    }

    // ELO History (Per Session)
    for (size_t i = 0; i < res.slots.size(); ++i) {
      auto* ps = stats_ptrs[i];

      bool update_needed = false;
      if (ps->elo_history.empty()) {
        update_needed = true;
      } else {
        auto& last = ps->elo_history.back();
        if (last.session_id != res.session_id) {
          update_needed = true;
        } else {
          last.timestamp_ms = res.timestamp_ms;
          last.value        = ps->current_elo;
        }
      }

      if (update_needed) {
        EloPoint ep;
        ep.session_id   = res.session_id;
        ep.timestamp_ms = res.timestamp_ms;
        ep.value        = ps->current_elo;
        ps->elo_history.push_back(ep);
      }
    }
  }

  LOG(INFO) << "Updating session info...";
  for (auto& [sid, info] : sessions) {
    if (sid.empty() || info.participants.empty())
      continue;
    for (const auto& pid : info.participants) {
      auto it = players.find(pid);
      if (it == players.end())
        continue;
      auto& ps = it->second;
      if (ps.processed_session_set.count(sid))
        continue;
      ps.processed_session_set.insert(sid);
      ps.processed_sessions.push_back(sid);
      ps.sessions_recorded++;
    }
  }

  int saved = 0;
  for (auto& [pid, ps] : players) {
    if (storage.save_json(pid, ToJson(ps)))
      saved++;
  }

  LOG(INFO) << "Player stats run complete. Records: " << processed_count
            << ", Players saved: " << saved;
  return true;
}

void PrintPlayerStatsSummary(const std::string& output_dir) {
  if (!fs::exists(output_dir) || !fs::is_directory(output_dir)) {
    return;
  }

  struct PlayerSummary {
    std::string id;
    std::string name;
    double elo;
    int rounds;
    int wins;
    double win_rate;
    double avg_shanten;
  };

  std::vector<PlayerSummary> summaries;

  for (const auto& entry : fs::directory_iterator(output_dir)) {
    if (entry.path().extension() != ".json") {
      continue;
    }

    std::ifstream ifs(entry.path());
    if (!ifs.is_open()) {
      continue;
    }

    try {
      json j;
      ifs >> j;

      PlayerSummary s;
      s.id          = j.value("player_id", "");
      s.name        = j.value("name", "");
      s.elo         = j.value("current_elo", 1500.0);
      s.rounds      = 0;
      s.wins        = 0;
      s.avg_shanten = 0.0;

      if (j.contains("stats") && j["stats"].is_object()) {
        const auto& stats = j["stats"];
        s.rounds          = stats.value("total_rounds", 0);
        s.wins            = stats.value("win_count", 0);
        s.avg_shanten     = stats.value("avg_starting_shanten", 0.0);
      }

      s.win_rate = (s.rounds > 0) ? (double)s.wins / s.rounds : 0.0;
      summaries.push_back(s);
    } catch (...) {
      continue;
    }
  }

  if (summaries.empty()) {
    return;
  }

  // Sort by ELO descending
  std::sort(summaries.begin(),
            summaries.end(),
            [](const PlayerSummary& a, const PlayerSummary& b) {
              return a.elo > b.elo;
            });

  std::cout << "\n=== Player Stats Summary (Top 20 by ELO) ===\n";
  std::cout << std::left << std::setw(12) << "ID" << std::setw(15) << "Name"
            << std::right << std::setw(8) << "ELO" << std::setw(8) << "Rounds"
            << std::setw(8) << "Wins" << std::setw(10) << "WinRate"
            << std::setw(10) << "Shanten"
            << "\n";
  std::cout << std::string(71, '-') << "\n";

  int count = 0;
  for (const auto& s : summaries) {
    std::cout << std::left << std::setw(12) << s.id.substr(0, 11)
              << std::setw(15) << s.name.substr(0, 14) << std::right
              << std::fixed << std::setprecision(1) << std::setw(8) << s.elo
              << std::setw(8) << s.rounds << std::setw(8) << s.wins
              << std::setprecision(1) << std::setw(9) << (s.win_rate * 100.0)
              << "%" << std::setprecision(2) << std::setw(10) << s.avg_shanten
              << "\n";
    if (++count >= 20) {
      break;
    }
  }
  std::cout << std::endl;
}

} // namespace stats
} // namespace tziakcha
