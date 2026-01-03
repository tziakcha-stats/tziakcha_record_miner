#include "analyzer/core.h"
#include "analyzer/record_parser.h"
#include "base/mahjong_constants.h"
#include <nlohmann/json.hpp>
#include <glog/logging.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <map>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;
using json   = nlohmann::json;

struct TestResult {
  std::string record_id;
  std::string filepath;
  bool success;
  int expected_fan;
  int calculated_fan;
  std::string winner_name;
  std::string error_message;
  bool is_draw;

  struct FanInfo {
    std::string fan_name;
    int fan_points;
    int count;
  };
  std::vector<FanInfo> gb_fan_details;

  std::vector<FanInfo> tziakcha_fan_details;
  int tziakcha_total_fan = -1;

  bool IsMatch() const {
    return success && !is_draw && (expected_fan == calculated_fan);
  }
  bool IsDraw() const { return is_draw; }
};

class FanCalculatorVerifier {
public:
  FanCalculatorVerifier(const std::string& record_dir)
      : record_dir_(record_dir) {}

  void Run() {
    std::cout << "========================================\n";
    std::cout << "  Fan Calculator Verification Tool\n";
    std::cout << "========================================\n\n";

    CollectRecordFiles();

    if (record_files_.empty()) {
      std::cerr << "No record files found in: " << record_dir_ << std::endl;
      return;
    }

    std::cout << "Found " << record_files_.size() << " record files\n\n";

    ProcessAllRecords();
    PrintSummary();
    SaveReport();
  }

private:
  void CollectRecordFiles() {
    if (!fs::exists(record_dir_)) {
      std::cerr << "Error: Directory not found: " << record_dir_ << std::endl;
      return;
    }

    for (const auto& entry : fs::directory_iterator(record_dir_)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        record_files_.push_back(entry.path().string());
      }
    }

    std::sort(record_files_.begin(), record_files_.end());
  }

  void ProcessAllRecords() {
    for (size_t i = 0; i < record_files_.size(); ++i) {
      const auto& filepath  = record_files_[i];
      std::string record_id = fs::path(filepath).stem().string();

      std::cout << "[" << std::setw(4) << (i + 1) << "/" << std::setw(4)
                << record_files_.size() << "] " << record_id << " ... ";
      std::cout.flush();

      TestResult result = ProcessSingleRecord(filepath, record_id);
      results_.push_back(result);

      if (result.IsDraw()) {
        std::cout << "○ DRAW (荒庄)\n";
      } else if (result.IsMatch()) {
        std::cout << "✓ PASS\n";
        PrintFanComparison(result);
      } else if (result.success) {
        std::cout << "✗ MISMATCH\n";
        PrintFanComparison(result);
      } else {
        std::cout << "✗ ERROR: " << result.error_message << "\n";
      }
    }
  }

  void PrintFanComparison(const TestResult& result) {
    std::cout << "    Winner: " << result.winner_name << "\n";
    std::cout << "    GB-Mahjong (" << result.calculated_fan << " fans):\n";
    for (const auto& fan : result.gb_fan_details) {
      std::cout << "      - " << fan.fan_name << ": " << fan.fan_points
                << "pt × " << fan.count << " = " << (fan.fan_points * fan.count)
                << "\n";
    }
    std::cout << "    tziakcha (" << result.expected_fan << " fans):\n";
    for (const auto& fan : result.tziakcha_fan_details) {
      std::cout << "      - " << fan.fan_name << ": " << fan.fan_points
                << "pt × " << fan.count << " = " << (fan.fan_points * fan.count)
                << "\n";
    }
    if (result.calculated_fan != result.expected_fan) {
      std::cout << "    Difference: "
                << (result.calculated_fan - result.expected_fan) << " fans\n";
    }
  }

  TestResult ProcessSingleRecord(const std::string& filepath,
                                 const std::string& record_id) {
    TestResult result;
    result.record_id      = record_id;
    result.filepath       = filepath;
    result.success        = false;
    result.expected_fan   = -1;
    result.calculated_fan = -1;
    result.is_draw        = false;

    try {
      std::ifstream file(filepath);
      if (!file.is_open()) {
        result.error_message = "Cannot open file";
        return result;
      }

      std::string record_json_str((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());

      if (record_json_str.empty()) {
        result.error_message = "Empty file";
        return result;
      }

      json record_json = json::parse(record_json_str);

      if (!record_json.contains("script")) {
        result.error_message = "No 'script' field in JSON";
        return result;
      }

      if (!record_json.contains("step")) {
        result.error_message = "No 'step' field in JSON";
        return result;
      }

      json script_data = record_json["step"];

      if (script_data.is_null() || !script_data.is_object()) {
        result.error_message = "Invalid 'step' field";
        return result;
      }

      if (!ExtractExpectedFan(script_data, result)) {
        return result;
      }

      try {
        tziakcha::analyzer::RecordAnalyzer analyzer;
        auto analysis_result = analyzer.Analyze(record_json_str);

        if (!analysis_result.success) {
          result.error_message =
              "Analysis failed: " + analysis_result.error_message;
          return result;
        }

        result.calculated_fan = analysis_result.win_analysis.total_fan;
        result.winner_name    = analysis_result.win_analysis.winner_name;
        result.success        = true;

        result.gb_fan_details.clear();
        for (const auto& detail : analysis_result.win_analysis.gb_fan_details) {
          result.gb_fan_details.push_back(
              {detail.fan_name, detail.fan_points, detail.count});
        }

        result.tziakcha_fan_details.clear();
        int tziakcha_total = 0;
        if (script_data.contains("y") && script_data["y"].is_array()) {
          int winner_idx = -1;
          int win_flags  = script_data["b"].get<int>();
          for (int i = 0; i < 4; ++i) {
            if ((win_flags & (1 << i)) != 0) {
              winner_idx = i;
              break;
            }
          }

          if (winner_idx >= 0 &&
              winner_idx < static_cast<int>(script_data["y"].size())) {
            auto win_info = script_data["y"][winner_idx];
            if (win_info.contains("t") && win_info["t"].is_object()) {
              for (auto& [fan_id_str, fan_val] : win_info["t"].items()) {
                int fan_id = std::stoi(fan_id_str);
                if (fan_id == 83)
                  continue;
                int fan_points = fan_val.get<int>() & 0xFF;
                int count      = ((fan_val.get<int>() >> 8) & 0xFF) + 1;

                std::string tziakcha_fan_name;
                if (fan_id >= 0 &&
                    fan_id <
                        static_cast<int>(tziakcha::base::FAN_NAMES.size())) {
                  tziakcha_fan_name = tziakcha::base::FAN_NAMES[fan_id];
                } else {
                  tziakcha_fan_name = "Fan" + std::to_string(fan_id);
                }

                result.tziakcha_fan_details.push_back(
                    {tziakcha_fan_name, fan_points, count});
                tziakcha_total += fan_points * count;
              }
            }
          }
        }
        result.tziakcha_total_fan = tziakcha_total;
        result.tziakcha_total_fan = tziakcha_total;

        std::ostringstream gb_log, tziakcha_log;
        gb_log << "[";
        bool first = true;
        for (const auto& fan : result.gb_fan_details) {
          if (!first)
            gb_log << ", ";
          gb_log << fan.fan_name << "(" << fan.fan_points << "×" << fan.count
                 << ")";
          first = false;
        }
        gb_log << "]";

        tziakcha_log << "[";
        first = true;
        for (const auto& fan : result.tziakcha_fan_details) {
          if (!first)
            tziakcha_log << ", ";
          tziakcha_log << fan.fan_name << "(" << fan.fan_points << "×"
                       << fan.count << ")";
          first = false;
        }
        tziakcha_log << "]";

        LOG(INFO) << record_id << ": GB=" << gb_log.str()
                  << " (Total: " << result.calculated_fan << ") | tziakcha="
                  << tziakcha_log.str() << " (Total: " << tziakcha_total << ")";

      } catch (const std::exception& e) {
        result.error_message = std::string("Analyzer exception: ") + e.what();
        return result;
      } catch (...) {
        result.error_message = "Analyzer unknown exception";
        return result;
      }

    } catch (const json::exception& e) {
      result.error_message = std::string("JSON exception: ") + e.what();
    } catch (const std::exception& e) {
      result.error_message = std::string("Exception: ") + e.what();
    } catch (...) {
      result.error_message = "Unknown exception";
    }

    return result;
  }

  bool ExtractExpectedFan(const json& script_data, TestResult& result) {
    if (!script_data.contains("b")) {
      result.error_message = "No 'b' field (win flags)";
      return false;
    }

    int win_flags  = script_data["b"].get<int>();
    int winner_idx = -1;

    for (int i = 0; i < 4; ++i) {
      if ((win_flags & (1 << i)) != 0) {
        winner_idx = i;
        break;
      }
    }

    if (winner_idx < 0) {
      result.is_draw        = true;
      result.success        = true;
      result.expected_fan   = 0;
      result.calculated_fan = 0;
      return true;
    }

    if (!script_data.contains("y") || !script_data["y"].is_array()) {
      result.error_message = "No 'y' field (win info)";
      return false;
    }

    if (winner_idx >= static_cast<int>(script_data["y"].size())) {
      result.error_message = "Winner index out of range";
      return false;
    }

    auto win_info = script_data["y"][winner_idx];

    if (!win_info.contains("f")) {
      result.error_message = "No 'f' field (fan count) in win info";
      return false;
    }

    result.expected_fan = win_info["f"].get<int>();

    return true;
  }

  void PrintSummary() {
    int total      = results_.size();
    int passed     = 0;
    int mismatched = 0;
    int errors     = 0;
    int draws      = 0;

    std::map<int, int> fan_diff_count;

    for (const auto& result : results_) {
      if (result.IsDraw()) {
        draws++;
      } else if (result.IsMatch()) {
        passed++;
      } else if (result.success) {
        mismatched++;
        int diff = std::abs(result.calculated_fan - result.expected_fan);
        fan_diff_count[diff]++;
      } else {
        errors++;
      }
    }

    std::cout << "\n========================================\n";
    std::cout << "  Verification Summary\n";
    std::cout << "========================================\n";
    std::cout << "Total records:     " << total << "\n";
    std::cout << "Passed (✓):        " << passed << " (" << std::fixed
              << std::setprecision(1) << (100.0 * passed / total) << "%)\n";
    std::cout << "Mismatched (✗):    " << mismatched << " ("
              << (100.0 * mismatched / total) << "%)\n";
    std::cout << "Draws (○):         " << draws << " ("
              << (100.0 * draws / total) << "%)\n";
    std::cout << "Errors:            " << errors << " ("
              << (100.0 * errors / total) << "%)\n";

    if (!fan_diff_count.empty()) {
      std::cout << "\nFan Difference Distribution:\n";
      for (const auto& [diff, count] : fan_diff_count) {
        std::cout << "  ±" << diff << " fan: " << count << " records\n";
      }
    }

    if (mismatched > 0) {
      std::cout << "\nMismatched Examples (first 10):\n";
      int shown = 0;
      for (const auto& result : results_) {
        if (!result.IsMatch() && result.success && shown < 10) {
          std::cout << "  " << result.record_id << ": Expected "
                    << result.expected_fan << ", Got " << result.calculated_fan
                    << " (Δ " << (result.calculated_fan - result.expected_fan)
                    << ")\n";
          shown++;
        }
      }
    }

    std::cout << "\n";
  }

  void SaveReport() {
    std::string report_path = "test/scripts/fan_verification_report.txt";
    std::ofstream report(report_path);

    if (!report.is_open()) {
      std::cerr << "Warning: Cannot write report to " << report_path
                << std::endl;
      return;
    }

    report << "Fan Calculator Verification Report\n";
    report << "Generated: " << GetCurrentTimeString() << "\n";
    report << "Record Directory: " << record_dir_ << "\n";
    report << "Total Records: " << results_.size() << "\n\n";

    report << "=== DRAW GAMES (荒庄) ===\n";
    for (const auto& result : results_) {
      if (result.IsDraw()) {
        report << result.record_id << "\n";
      }
    }

    report << "\n=== PASSED RECORDS ===\n";
    for (const auto& result : results_) {
      if (result.IsMatch()) {
        report << result.record_id << "\t" << result.expected_fan << "\n";
      }
    }

    report << "\n=== MISMATCHED RECORDS ===\n";
    for (const auto& result : results_) {
      if (!result.IsMatch() && result.success) {
        report << result.record_id << "\t" << result.winner_name << "\n";
        report << "  Expected: " << result.expected_fan << " fans\n";
        report << "  Calculated: " << result.calculated_fan << " fans\n";
        report << "  Difference: "
               << (result.calculated_fan - result.expected_fan) << " fans\n";
        report << "  GB-Mahjong details:\n";
        for (const auto& fan : result.gb_fan_details) {
          report << "    - " << fan.fan_name << ": " << fan.fan_points
                 << "pt × " << fan.count << " = "
                 << (fan.fan_points * fan.count) << "\n";
        }
        report << "  tziakcha details:\n";
        for (const auto& fan : result.tziakcha_fan_details) {
          report << "    - " << fan.fan_name << ": " << fan.fan_points
                 << "pt × " << fan.count << " = "
                 << (fan.fan_points * fan.count) << "\n";
        }
        report << "\n";
      }
    }

    report << "\n=== ERROR RECORDS ===\n";
    report << "RecordID\tError\n";
    for (const auto& result : results_) {
      if (!result.success) {
        report << result.record_id << "\t" << result.error_message << "\n";
      }
    }

    report.close();
    std::cout << "Detailed report saved to: " << report_path << "\n";
  }

  std::string GetCurrentTimeString() {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
  }

  std::string record_dir_;
  std::vector<std::string> record_files_;
  std::vector<TestResult> results_;
};

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);

  std::string record_dir = "data/record";
  bool verbose           = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [OPTIONS] [RECORD_DIR]\n\n";
      std::cout << "Options:\n";
      std::cout << "  -v, --verbose    Enable verbose logging output\n";
      std::cout << "  -h, --help       Show this help message\n\n";
      std::cout << "Arguments:\n";
      std::cout << "  RECORD_DIR       Directory containing record JSON files "
                   "(default: data/record)\n\n";
      return 0;
    } else if (arg[0] != '-') {
      record_dir = arg;
    }
  }

  if (verbose) {
    FLAGS_minloglevel = 0;

    const char* log_dir = std::getenv("GLOG_log_dir");
    if (log_dir && std::strlen(log_dir) > 0) {
      FLAGS_alsologtostderr = true;
      std::cout << "Verbose logging enabled\n";
      std::cout << "Glog directory: " << log_dir << "\n\n";
    } else {
      FLAGS_logtostderr = true;
      std::cout << "Verbose logging enabled\n\n";
    }
  } else {
    FLAGS_logtostderr = false;
    FLAGS_minloglevel = 2;
  }

  FanCalculatorVerifier verifier(record_dir);
  verifier.Run();

  return 0;
}
