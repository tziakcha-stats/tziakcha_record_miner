#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <cxxopts.hpp>
#include <glog/logging.h>

#include "analyzer/simulator.h"
#include "stats/intercept_stats.h"
#include "stats/player_stats.h"

namespace fs = std::filesystem;

namespace {
bool ReadFileToString(const fs::path& path, std::string& out) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return false;
  }
  std::ostringstream ss;
  ss << ifs.rdbuf();
  out = ss.str();
  return true;
}
} // namespace

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

  cxxopts::Options options(
      "stats_cli", "Mahjong intercept (\u622a\u548c) statistics CLI");
  options.add_options()(
      "d,dir",
      "Record directory",
      cxxopts::value<std::string>()->default_value("data/record"))(
      "l,limit",
      "Maximum files to process (0 = all)",
      cxxopts::value<int>()->default_value("0"))(
      "v,verbose",
      "Enable verbose logging",
      cxxopts::value<bool>()->default_value("false"))(
      "player-stats",
      "Run player statistics aggregation",
      cxxopts::value<bool>()->default_value("false"))(
      "player-dir",
      "Output directory for player stats",
      cxxopts::value<std::string>()->default_value("data/player"))(
      "session-map",
      "Optional session map file (currently unused, reserved for future)",
      cxxopts::value<std::string>()->default_value(
          "data/sessions/all_record.json"))(
      "list-events",
      "Print intercept events",
      cxxopts::value<bool>()->default_value("false"))("h,help", "Show help");

  auto result = options.parse(argc, argv);
  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  bool verbose = result["verbose"].as<bool>();
  if (verbose) {
    FLAGS_v               = 1;
    FLAGS_logtostderr     = true;
    FLAGS_alsologtostderr = true;
  } else {
    FLAGS_logtostderr     = false;
    FLAGS_alsologtostderr = false;
    FLAGS_minloglevel     = 0;
  }

  fs::path dir     = result["dir"].as<std::string>();
  int limit        = result["limit"].as<int>();
  bool list_events = result["list-events"].as<bool>();
  bool player_mode = result["player-stats"].as<bool>();

  if (!fs::exists(dir) || !fs::is_directory(dir)) {
    std::cerr << "Record directory not found: " << dir << std::endl;
    return 1;
  }

  if (player_mode) {
    tziakcha::stats::PlayerStatsOptions ps_opts;
    ps_opts.record_dir       = dir.string();
    ps_opts.output_dir       = result["player-dir"].as<std::string>();
    ps_opts.session_map_path = result["session-map"].as<std::string>();
    ps_opts.limit            = limit;
    ps_opts.verbose          = verbose;

    bool ok = tziakcha::stats::RunPlayerStats(ps_opts);
    if (!ok) {
      std::cerr << "Player stats run failed" << std::endl;
      return 1;
    }

    std::cout << "Player stats written under: " << ps_opts.output_dir
              << std::endl;
    return 0;
  }

  tziakcha::analyzer::RecordSimulator simulator;
  tziakcha::stats::InterceptStats intercept_stats;

  int last_discard_player = -1;
  int last_discard_step   = -1;
  int last_discard_tile   = -1;
  int last_draw_player    = -1;
  int last_draw_step      = -1;
  bool file_has_win       = false;
  bool file_has_tsumo     = false;
  bool file_has_ron       = false;
  enum class RoundResult { None, Draw, Tsumo, Ron };
  RoundResult last_result = RoundResult::None;
  bool has_ron_event      = false;
  tziakcha::stats::InterceptEvent last_ron_event{};

  simulator.ClearActionObservers();
  simulator.AddActionObserver(
      [&intercept_stats,
       &simulator,
       &last_discard_player,
       &last_discard_step,
       &last_discard_tile,
       &last_draw_player,
       &last_draw_step,
       &file_has_win,
       &file_has_tsumo,
       &file_has_ron,
       &last_result,
       &has_ron_event,
       &last_ron_event](const tziakcha::analyzer::Action& action,
                        int step,
                        const tziakcha::analyzer::GameState& state) {
        switch (action.action_type) {
        case 2:
          last_discard_player = action.player_idx;
          last_discard_step   = step;
          last_discard_tile   = state.GetLastDiscardTile();
          break;
        case 1:
        case 7:
          last_draw_player = action.player_idx;
          last_draw_step   = step;
          break;
        case 6: {
          int fan = action.data >> 1;
          if (fan <= 0) {
            break;
          }

          file_has_win = true;

          bool is_self_drawn = (last_draw_player == action.player_idx) &&
                               (last_draw_step > last_discard_step);

          if (is_self_drawn) {
            file_has_tsumo = true;
            last_result    = RoundResult::Tsumo;
            break;
          }

          file_has_ron = true;
          last_result  = RoundResult::Ron;

          int discarder_idx = state.GetLastDiscardPlayer();
          int discard_tile  = state.GetLastDiscardTile();

          if (discarder_idx < 0 || discard_tile < 0) {
            LOG(WARNING) << "Skip intercept check: missing discarder info";
            break;
          }

          int round_wind_index = simulator.GetRoundWindIndex();
          auto ev              = intercept_stats.CheckIntercept(
              discarder_idx,
              discard_tile,
              state,
              state.GetDealerIdx(),
              round_wind_index,
              step);
          has_ron_event  = true;
          last_ron_event = ev;
          break;
        }
        default:
          break;
        }
      });

  int files_seen     = 0;
  int files_success  = 0;
  int total_ron_wins = 0;
  int intercept_cnt  = 0;
  int total_events   = 0;
  int draw_rounds    = 0;
  int tsumo_rounds   = 0;
  int ron_rounds     = 0;
  int ron_calc_ok    = 0;
  int ron_calc_fail  = 0;

  for (auto it = fs::recursive_directory_iterator(dir);
       it != fs::recursive_directory_iterator();
       ++it) {
    if (!it->is_regular_file()) {
      continue;
    }
    if (it->path().extension() != ".json") {
      continue;
    }
    if (limit > 0 && files_seen >= limit) {
      break;
    }

    ++files_seen;
    const auto& path = it->path();

    std::string content;
    if (!ReadFileToString(path, content)) {
      LOG(ERROR) << "Failed to read file: " << path;
      continue;
    }

    intercept_stats.Reset();
    intercept_stats.SetRoundId(path.filename().string());
    last_discard_player = -1;
    last_discard_step   = -1;
    last_discard_tile   = -1;
    last_draw_player    = -1;
    last_draw_step      = -1;
    file_has_win        = false;
    file_has_tsumo      = false;
    file_has_ron        = false;
    last_result         = RoundResult::None;
    has_ron_event       = false;

    auto res = simulator.Simulate(content);
    if (!res.success) {
      LOG(WARNING) << "Simulation failed for " << path << ": "
                   << res.error_message;
      continue;
    }

    if (last_result == RoundResult::Ron && has_ron_event &&
        !last_ron_event.potential_winners.empty()) {
      intercept_stats.AddEvent(last_ron_event);
    }

    ++files_success;
    const auto& stats = intercept_stats.GetResult();
    total_ron_wins += stats.total_ron_wins;
    intercept_cnt += stats.intercept_count;
    total_events += static_cast<int>(stats.events.size());

    if (!file_has_win || last_result == RoundResult::None) {
      ++draw_rounds;
    } else if (last_result == RoundResult::Tsumo) {
      ++tsumo_rounds;
    } else if (last_result == RoundResult::Ron) {
      ++ron_rounds;
      if (has_ron_event && !last_ron_event.potential_winners.empty()) {
        ++ron_calc_ok;
      } else {
        ++ron_calc_fail;
      }
    }

    if (list_events && stats.intercept_count > 0) {
      std::cout << "\n[File] " << path << "\n";
      for (const auto& ev : stats.events) {
        if (ev.is_intercept) {
          std::cout << ev.ToString() << "\n";
        }
      }
    }
  }

  double intercept_rate = 0.0;
  if (total_ron_wins > 0) {
    intercept_rate = static_cast<double>(intercept_cnt) / total_ron_wins;
  }

  std::cout << "\n=== Intercept Stats Summary ===\n";
  std::cout << "Files scanned: " << files_seen << " (success: " << files_success
            << ")\n";
  std::cout << "Ron wins: " << total_ron_wins << "\n";
  std::cout << "Intercepts: " << intercept_cnt << "\n";
  std::cout << "Intercept rate: " << intercept_rate * 100.0 << "%\n";
  std::cout << "Rounds - Draw: " << draw_rounds << ", Self-draw: "
            << tsumo_rounds << ", Ron: " << ron_rounds << "\n";
  std::cout << "Ron calc success: " << ron_calc_ok
            << ", Ron calc failed/invalid: " << ron_calc_fail << "\n";
  std::cout << "Events recorded: " << total_events << "\n";

  return 0;
}
