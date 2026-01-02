#pragma once

#include "analyzer/simulator.h"
#include "utils/tile.h"
#include <iostream>
#include <string>
#include <vector>

namespace tziakcha {
namespace analyzer {

class RecordPrinter {
public:
  static void PrintWinAnalysis(const WinAnalysis& analysis) {
    std::cout << "\n=== Win Analysis ===" << std::endl;
    std::cout << "Winner: " << analysis.winner_name << " ("
              << analysis.winner_wind << ")" << std::endl;
    std::cout << "Total Fan (from script): " << analysis.total_fan << std::endl;
    std::cout << "Base Fan (from script): " << analysis.base_fan << std::endl;
    std::cout << "Calculated Fan (from GB-Mahjong): " << analysis.calculated_fan
              << std::endl;
    std::cout << "Flower Count: " << analysis.flower_count << std::endl;
    std::cout << "Formatted Hand: " << analysis.formatted_hand << std::endl;
    std::cout << "Env Flag: " << analysis.env_flag << std::endl;
    std::cout << "Hand String For GB: " << analysis.hand_string_for_gb
              << std::endl;

    if (!analysis.fan_details.empty()) {
      std::cout << "\nFan Details (from script):" << std::endl;
      for (const auto& fan : analysis.fan_details) {
        std::cout << "  " << fan.fan_name << ": " << fan.fan_points << " fan";
        if (fan.count > 1) {
          std::cout << " x" << fan.count;
        }
        std::cout << std::endl;
      }
    }
  }

  static void PrintSimulationResult(const SimulationResult& result) {
    if (result.success) {
      std::cout << "Simulation successful!" << std::endl;
      PrintWinAnalysis(result.win_analysis);
    } else {
      std::cout << "Simulation failed: " << result.error_message << std::endl;
    }
  }

  static void PrintGameLog(const GameLog& game_log) {
    std::cout << "\n=== Game Log ===" << std::endl;
    std::cout << "Game: " << game_log.game_title << std::endl;
    std::cout << "Players:" << std::endl;
    for (size_t i = 0; i < game_log.player_names.size(); ++i) {
      std::cout << "  " << static_cast<char>('E' + i) << ": "
                << game_log.player_names[i] << std::endl;
    }
    std::cout << "\nTotal Steps: " << game_log.step_logs.size() << std::endl;
  }

  static void PrintStepLogs(const std::vector<StepLog>& step_logs) {
    std::cout << "\n=== Game Steps ===" << std::endl;
    for (const auto& step : step_logs) {
      PrintStep(step);
    }
  }

  static void PrintStep(const StepLog& step) {
    std::cout << "\n[Step " << step.step_number << "] " << step.player_wind
              << "家 " << step.player_name << " (+"
              << step.time_elapsed_ms / 1000.0 << "s)" << std::endl;
    std::cout << "Action: " << step.action_description << std::endl;

    std::cout << "Hand: ";
    for (const auto& tile : step.hand_tiles) {
      std::cout << tile << " ";
    }
    std::cout << std::endl;

    if (!step.pack_tiles.empty()) {
      std::cout << "Packs: ";
      for (const auto& pack : step.pack_tiles) {
        std::cout << pack << " ";
      }
      std::cout << std::endl;
    }

    std::cout << "Discards: ";
    for (const auto& tile : step.discard_tiles) {
      std::cout << tile << " ";
    }
    std::cout << std::endl;
  }

  static void PrintDetailedAnalysis(const SimulationResult& result) {
    if (!result.success) {
      std::cout << "Simulation failed: " << result.error_message << std::endl;
      return;
    }

    PrintGameLog(result.game_log);
    PrintStepLogs(result.game_log.step_logs);
    PrintWinAnalysis(result.win_analysis);
  }
};

} // namespace analyzer
} // namespace tziakcha
