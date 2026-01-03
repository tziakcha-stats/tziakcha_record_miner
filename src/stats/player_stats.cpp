#include "stats/player_stats.h"

#include "base/mahjong_constants.h"
#include "stats/player_stats_config.h"
#include "storage/filesystem_storage.h"
#include "utils/script_decoder.h"
#include "analyzer/simulator.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <glog/logging.h>
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
  std::string record_id;
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
  std::vector<FanSummary> max_fans;
};

struct PlayerStats {
  std::string player_id;
  std::string name;
  double current_elo = 1500.0;

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

  std::vector<EloPoint> elo_history;
  std::vector<std::string> processed_sessions;
  std::vector<std::string> processed_records;
  std::vector<WinEntry> wins;

  std::unordered_set<std::string> processed_session_set;
  std::unordered_set<std::string> processed_record_set;
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

    int combined   = act[0].get<int>();
    int player_idx = (combined >> 4) & 3;
    if (player_idx >= 0 &&
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
  obj["record_id"]    = w.record_id;
  obj["session_id"]   = w.session_id;
  obj["timestamp_ms"] = w.timestamp_ms;
  obj["win_type"]     = w.win_type;
  obj["total_fan"]    = w.total_fan;
  obj["hand_raw"]     = w.hand_raw;
  obj["max_fans"]     = json::array();
  for (const auto& f : w.max_fans) {
    obj["max_fans"].push_back(ToJson(f));
  }
  return obj;
}

json ToJson(const EloPoint& e) {
  return json{{"record_id", e.record_id},
              {"session_id", e.session_id},
              {"timestamp_ms", e.timestamp_ms},
              {"elo", e.value}};
}

PlayerStats FromJson(const json& j) {
  PlayerStats ps;
  ps.player_id   = j.value("player_id", "");
  ps.name        = j.value("name", "");
  ps.current_elo = j.value("current_elo", 1500.0);

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

  if (j.contains("elo_history") && j["elo_history"].is_array()) {
    for (const auto& item : j["elo_history"]) {
      EloPoint ep;
      ep.record_id    = item.value("record_id", "");
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

  if (j.contains("processed_records") && j["processed_records"].is_array()) {
    for (const auto& r : j["processed_records"]) {
      if (r.is_string()) {
        auto val = r.get<std::string>();
        ps.processed_records.push_back(val);
        ps.processed_record_set.insert(val);
      }
    }
  }

  if (j.contains("wins") && j["wins"].is_array()) {
    for (const auto& w : j["wins"]) {
      WinEntry we;
      we.record_id    = w.value("record_id", "");
      we.session_id   = w.value("session_id", "");
      we.timestamp_ms = w.value("timestamp_ms", 0LL);
      we.win_type     = w.value("win_type", "");
      we.total_fan    = w.value("total_fan", 0);
      we.hand_raw     = w.value("hand_raw", "");

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

json ToJson(const PlayerStats& ps) {
  json j;
  j["player_id"]   = ps.player_id;
  j["name"]        = ps.name;
  j["current_elo"] = ps.current_elo;

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
  j["stats"] = stats_obj;

  j["elo_history"] = json::array();
  for (const auto& ep : ps.elo_history) {
    j["elo_history"].push_back(ToJson(ep));
  }

  j["processed_sessions"] = ps.processed_sessions;
  j["processed_records"]  = ps.processed_records;

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

  (void)options.session_map_path;
  (void)options.verbose;

  storage::FileSystemStorage storage(options.output_dir);

  std::vector<RecordMeta> records;
  for (auto it = fs::recursive_directory_iterator(record_dir);
       it != fs::recursive_directory_iterator();
       ++it) {
    if (!it->is_regular_file() || it->path().extension() != ".json") {
      continue;
    }
    if (options.limit > 0 &&
        static_cast<int>(records.size()) >= options.limit) {
      break;
    }

    std::string content;
    if (!ReadFile(it->path(), content)) {
      LOG(WARNING) << "Failed to read record: " << it->path();
      continue;
    }
    auto record_json = ParseJson(content, it->path());
    json script_json;
    if (record_json.contains("step") && record_json["step"].is_object()) {
      script_json = record_json["step"];
    } else if (!DecodeScript(record_json, script_json)) {
      LOG(WARNING) << "Failed to decode script for " << it->path();
      continue;
    }

    RecordMeta meta;
    meta.path        = it->path();
    meta.content     = std::move(content);
    meta.script_json = script_json;
    meta.raw_json    = record_json;
    meta.record_id   = record_json.value(
        PlayerStatsConfig::kRecordId, it->path().stem().string());
    meta.session_id   = record_json.value(PlayerStatsConfig::kSessionId, "");
    meta.timestamp_ms = GetRecordTimestamp(record_json);

    records.push_back(std::move(meta));
  }

  std::sort(records.begin(),
            records.end(),
            [](const RecordMeta& a, const RecordMeta& b) {
              return a.timestamp_ms < b.timestamp_ms;
            });

  std::unordered_map<std::string, PlayerStats> players;
  std::unordered_map<std::string, SessionInfo> sessions;

  auto get_player = [&](const PlayerSlot& slot) -> PlayerStats& {
    auto it = players.find(slot.id);
    if (it != players.end()) {
      if (!slot.name.empty()) {
        it->second.name = slot.name;
      }
      return it->second;
    }

    PlayerStats ps;
    ps.player_id = slot.id;
    ps.name      = slot.name;

    json existing;
    if (storage.load_json(slot.id, existing)) {
      ps = FromJson(existing);
      if (!slot.name.empty()) {
        ps.name = slot.name;
      }

      if (ps.total_session_seconds < 0.0) {
        LOG(WARNING) << "Resetting stats for player " << slot.id
                     << " due to negative total_session_seconds="
                     << ps.total_session_seconds;
        PlayerStats reset;
        reset.player_id = slot.id;
        reset.name      = ps.name;
        ps              = std::move(reset);
      }
    }

    auto [iter, _] = players.emplace(slot.id, std::move(ps));
    return iter->second;
  };

  int processed_records = 0;
  for (const auto& record : records) {
    auto slots = ExtractPlayers(record.script_json);
    if (slots.empty()) {
      continue;
    }

    auto durations   = ComputeActionDurations(record.raw_json);
    auto step_counts = CountStepsByPlayer(record.raw_json);

    const json* player_e_src = nullptr;
    if (record.raw_json.contains(PlayerStatsConfig::kStep) &&
        record.raw_json[PlayerStatsConfig::kStep].is_object()) {
      const auto& step_obj = record.raw_json[PlayerStatsConfig::kStep];
      if (step_obj.contains(PlayerStatsConfig::kStepPlayers) &&
          step_obj[PlayerStatsConfig::kStepPlayers].is_array()) {
        player_e_src = &step_obj[PlayerStatsConfig::kStepPlayers];
      }
    } else if (record.script_json.contains(PlayerStatsConfig::kScriptPlayers) &&
               record.script_json[PlayerStatsConfig::kScriptPlayers]
                   .is_array()) {
      player_e_src = &record.script_json[PlayerStatsConfig::kScriptPlayers];
    }

    if (player_e_src) {
      const auto& sp = *player_e_src;
      for (size_t i = 0; i < sp.size() && i < slots.size(); ++i) {
        if (!sp[i].is_object()) {
          continue;
        }
        double elo_val = sp[i].value(PlayerStatsConfig::kEloField, 1500.0);
        auto& ps       = get_player(slots[i]);
        ps.current_elo = elo_val;
      }
    }

    std::vector<PlayerStats*> stats_ptrs;
    stats_ptrs.reserve(slots.size());
    bool any_processed = false;
    bool all_processed = true;
    for (const auto& slot : slots) {
      auto& ps = get_player(slot);
      stats_ptrs.push_back(&ps);
      if (ps.processed_record_set.count(record.record_id) > 0) {
        any_processed = true;
      } else {
        all_processed = false;
      }
    }

    if (any_processed && all_processed) {
      continue;
    }
    if (any_processed && !all_processed) {
      LOG(WARNING) << "Record " << record.record_id
                   << " already processed for some players, skipping to keep"
                   << " ratings consistent.";
      continue;
    }

    const auto flag_info = ParseWinFlags(record.script_json);
    const int winner_idx =
        flag_info.winners.empty() ? -1 : flag_info.winners.front();
    const bool is_draw = winner_idx < 0;
    const bool is_self_drawn =
        (!is_draw &&
         (flag_info.discarder < 0 || flag_info.discarder == winner_idx));

    json win_data;
    if (!is_draw &&
        record.script_json.contains(PlayerStatsConfig::kScriptWins) &&
        record.script_json[PlayerStatsConfig::kScriptWins].is_array() &&
        winner_idx <
            static_cast<int>(
                record.script_json[PlayerStatsConfig::kScriptWins].size())) {
      win_data = record.script_json[PlayerStatsConfig::kScriptWins][winner_idx];
    }
    int total_fan = win_data.is_object()
                      ? win_data.value(PlayerStatsConfig::kWinFanTotal, 0)
                      : 0;
    auto max_fans = ExtractMaxFans(win_data);

    auto& session  = sessions[record.session_id];
    session.min_ts = std::min(session.min_ts, record.timestamp_ms);
    session.max_ts = std::max(session.max_ts, record.timestamp_ms);
    session.duration_ms += durations.record_ms;

    std::string gb_hand_str;
    if (!is_draw) {
      analyzer::RecordSimulator simulator;
      auto sim_result = simulator.Simulate(record.content);
      if (sim_result.success &&
          sim_result.win_analysis.winner_idx == winner_idx) {
        gb_hand_str = sim_result.win_analysis.hand_string_for_gb;
      }
    }

    for (size_t i = 0; i < slots.size(); ++i) {
      auto& ps = *stats_ptrs[i];
      ps.name  = slots[i].name;

      ps.total_rounds++;
      ps.processed_record_set.insert(record.record_id);
      ps.processed_records.push_back(record.record_id);
      int64_t player_steps =
          static_cast<int>(i) < static_cast<int>(step_counts.size())
              ? step_counts[i]
              : 0;
      ps.total_steps += player_steps;
      double player_step_seconds =
          static_cast<int>(i) < static_cast<int>(durations.player_ms.size())
              ? static_cast<double>(durations.player_ms[i]) / 1000.0
              : 0.0;
      ps.total_session_seconds += player_step_seconds;

      session.participants.insert(ps.player_id);

      if (is_draw) {
        ps.draw_count++;
        continue;
      }

      if (static_cast<int>(i) == winner_idx) {
        ps.win_count++;
        if (is_self_drawn) {
          ps.tsumo_win_count++;
        } else {
          ps.ron_win_count++;
        }

        WinEntry win_entry;
        win_entry.record_id    = record.record_id;
        win_entry.session_id   = record.session_id;
        win_entry.timestamp_ms = record.timestamp_ms;
        win_entry.win_type     = is_self_drawn ? "tsumo" : "ron";
        win_entry.total_fan    = total_fan;
        if (!gb_hand_str.empty()) {
          win_entry.hand_raw = gb_hand_str;
        } else if (win_data.contains(PlayerStatsConfig::kWinHand)) {
          try {
            win_entry.hand_raw = win_data[PlayerStatsConfig::kWinHand].dump();
          } catch (const std::exception&) {
            win_entry.hand_raw.clear();
          }
        }
        win_entry.max_fans = max_fans;
        ps.wins.push_back(std::move(win_entry));
      } else {
        if (!is_self_drawn && flag_info.discarder == static_cast<int>(i)) {
          ps.deal_in_count++;
        }
        if (is_self_drawn) {
          ps.tsumo_against_count++;
        }
      }
    }

    for (size_t i = 0; i < slots.size(); ++i) {
      auto* ps = stats_ptrs[i];
      EloPoint ep;
      ep.record_id    = record.record_id;
      ep.session_id   = record.session_id;
      ep.timestamp_ms = record.timestamp_ms;
      ep.value        = ps->current_elo;
      ps->elo_history.push_back(ep);
    }

    processed_records++;
  }

  for (auto& [session_id, info] : sessions) {
    if (session_id.empty() || info.participants.empty()) {
      continue;
    }
    for (const auto& pid : info.participants) {
      auto it = players.find(pid);
      if (it == players.end()) {
        continue;
      }
      auto& ps = it->second;
      if (ps.processed_session_set.count(session_id) > 0) {
        continue;
      }
      ps.processed_session_set.insert(session_id);
      ps.processed_sessions.push_back(session_id);
      ps.sessions_recorded++;
    }
  }

  int saved = 0;
  for (auto& [pid, ps] : players) {
    if (storage.save_json(pid, ToJson(ps))) {
      saved++;
    }
  }

  LOG(INFO) << "Player stats processed records: " << processed_records
            << ", players saved: " << saved;

  return true;
}

} // namespace stats
} // namespace tziakcha
