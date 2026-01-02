#include "analyzer/core.h"
#include "analyzer/record_printer.h"
#include <cxxopts.hpp>
#include <glog/logging.h>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

std::string ReadFile(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + filepath);
  }
  return std::string(
      (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void PrintUsage() {
  std::cout << "Tziakcha Record Analyzer - Analyzer CLI\n\n";
  std::cout << "Usage:\n";
  std::cout << "  analyzer_cli <command> [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  analyze     Analyze a single mahjong record\n";
  std::cout << "  batch       Analyze multiple records from a directory\n";
  std::cout << "  help        Show this help message\n\n";
  std::cout << "Run 'analyzer_cli <command> --help' for more information on a "
               "command.\n";
}

int CmdAnalyze(int argc, char* argv[]) {
  cxxopts::Options options(
      "analyzer_cli analyze", "Analyze a single mahjong record");
  options.add_options()(
      "f,file", "Path to record JSON file", cxxopts::value<std::string>())(
      "j,json",
      "Record JSON string (if not using file)",
      cxxopts::value<std::string>())(
      "p,print",
      "Print detailed analysis to console",
      cxxopts::value<bool>()->default_value("true"))(
      "d,detailed",
      "Print detailed game steps",
      cxxopts::value<bool>()->default_value("false"))(
      "o,output", "Output file path (optional)", cxxopts::value<std::string>())(
      "v,verbose",
      "Enable verbose logging",
      cxxopts::value<bool>()->default_value("false"))("h,help", "Print help");

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (result.count("verbose")) {
    FLAGS_logtostderr = 1;
    FLAGS_minloglevel = 0;
  }

  std::string record_json_str;

  if (result.count("file")) {
    std::string filepath = result["file"].as<std::string>();
    if (!fs::exists(filepath)) {
      std::cerr << "Error: File not found: " << filepath << std::endl;
      return 1;
    }

    try {
      record_json_str = ReadFile(filepath);
      LOG(INFO) << "Loaded record from file: " << filepath;
    } catch (const std::exception& e) {
      std::cerr << "Error reading file: " << e.what() << std::endl;
      return 1;
    }
  } else if (result.count("json")) {
    record_json_str = result["json"].as<std::string>();
    LOG(INFO) << "Using provided JSON string";
  } else {
    std::cerr << "Error: Either --file or --json is required" << std::endl;
    std::cout << options.help() << std::endl;
    return 1;
  }

  try {
    auto& analyzer       = tziakcha::analyzer::RecordAnalyzer::GetInstance();
    auto analysis_result = analyzer.Analyze(record_json_str);

    if (!analysis_result.success) {
      std::cerr << "Analysis failed: " << analysis_result.error_message
                << std::endl;
      LOG(ERROR) << "Analysis failed: " << analysis_result.error_message;
      return 1;
    }

    if (result["print"].as<bool>()) {
      tziakcha::analyzer::RecordPrinter::PrintWinAnalysis(
          analysis_result.win_analysis);
    }

    if (result["detailed"].as<bool>()) {
      tziakcha::analyzer::RecordPrinter::PrintDetailedAnalysis(analysis_result);
    }

    if (result.count("output")) {
      std::string output_file = result["output"].as<std::string>();
      std::ofstream out(output_file);
      if (!out.is_open()) {
        std::cerr << "Error: Cannot write to output file: " << output_file
                  << std::endl;
        return 1;
      }

      const auto& win_info = analysis_result.win_analysis;
      out << "{\n";
      out << "  \"winner_name\": \"" << win_info.winner_name << "\",\n";
      out << "  \"winner_wind\": \"" << win_info.winner_wind << "\",\n";
      out << "  \"total_fan\": " << win_info.total_fan << ",\n";
      out << "  \"base_fan\": " << win_info.base_fan << ",\n";
      out << "  \"flower_count\": " << win_info.flower_count << ",\n";
      out << "  \"env_flag\": \"" << win_info.env_flag << "\",\n";
      out << "  \"hand_string_for_gb\": \"" << win_info.hand_string_for_gb
          << "\",\n";
      out << "  \"fan_details\": [\n";

      for (size_t i = 0; i < win_info.fan_details.size(); ++i) {
        const auto& fan = win_info.fan_details[i];
        out << "    {\n";
        out << "      \"fan_id\": " << fan.fan_id << ",\n";
        out << "      \"fan_name\": \"" << fan.fan_name << "\",\n";
        out << "      \"fan_points\": " << fan.fan_points << ",\n";
        out << "      \"count\": " << fan.count << "\n";
        out << "    }";
        if (i < win_info.fan_details.size() - 1) {
          out << ",";
        }
        out << "\n";
      }

      out << "  ]\n";
      out << "}\n";
      out.close();

      std::cout << "Analysis result written to: " << output_file << std::endl;
      LOG(INFO) << "Analysis result written to: " << output_file;
    }

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    LOG(ERROR) << "Exception: " << e.what();
    return 1;
  }
}

int CmdBatch(int argc, char* argv[]) {
  cxxopts::Options options(
      "analyzer_cli batch", "Analyze multiple records from a directory");
  options.add_options()("d,directory",
                        "Directory containing record JSON files",
                        cxxopts::value<std::string>())(
      "p,pattern",
      "File pattern to match (default: *.json)",
      cxxopts::value<std::string>()->default_value("*.json"))(
      "s,summary", "Output summary to file", cxxopts::value<std::string>())(
      "v,verbose",
      "Enable verbose logging",
      cxxopts::value<bool>()->default_value("false"))("h,help", "Print help");

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (!result.count("directory")) {
    std::cerr << "Error: --directory is required" << std::endl;
    std::cout << options.help() << std::endl;
    return 1;
  }

  if (result.count("verbose")) {
    FLAGS_logtostderr = 1;
    FLAGS_minloglevel = 0;
  }

  std::string directory = result["directory"].as<std::string>();
  std::string pattern   = result["pattern"].as<std::string>();

  if (!fs::is_directory(directory)) {
    std::cerr << "Error: Not a directory: " << directory << std::endl;
    return 1;
  }

  auto& analyzer = tziakcha::analyzer::RecordAnalyzer::GetInstance();

  std::vector<std::string> json_files;
  int success_count = 0;
  int error_count   = 0;

  std::cout << "Scanning directory: " << directory << std::endl;

  for (const auto& entry : fs::recursive_directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      json_files.push_back(entry.path().string());
    }
  }

  std::cout << "Found " << json_files.size() << " JSON files to process"
            << std::endl;
  std::cout << std::endl;

  std::ofstream summary_file;
  if (result.count("summary")) {
    std::string summary_path = result["summary"].as<std::string>();
    summary_file.open(summary_path);
    if (!summary_file.is_open()) {
      std::cerr << "Warning: Cannot write to summary file: " << summary_path
                << std::endl;
    }
  }

  for (size_t i = 0; i < json_files.size(); ++i) {
    const auto& filepath = json_files[i];

    std::cout << "[" << (i + 1) << "/" << json_files.size()
              << "] Processing: " << filepath << std::endl;

    try {
      std::string record_json_str = ReadFile(filepath);
      auto analysis_result        = analyzer.Analyze(record_json_str);

      if (analysis_result.success) {
        const auto& win_info = analysis_result.win_analysis;
        std::cout << "  ✓ Winner: " << win_info.winner_name
                  << " | Fan: " << win_info.total_fan << std::endl;

        if (summary_file.is_open()) {
          summary_file << filepath << "\t" << win_info.winner_name << "\t"
                       << win_info.total_fan << "\t" << win_info.base_fan
                       << "\t" << win_info.flower_count << "\n";
        }

        success_count++;
      } else {
        std::cerr << "  ✗ Error: " << analysis_result.error_message
                  << std::endl;
        error_count++;
      }

    } catch (const std::exception& e) {
      std::cerr << "  ✗ Exception: " << e.what() << std::endl;
      error_count++;
    }
  }

  std::cout << std::endl;
  std::cout << "========== Batch Analysis Summary ==========" << std::endl;
  std::cout << "Total files: " << json_files.size() << std::endl;
  std::cout << "Success: " << success_count << std::endl;
  std::cout << "Failed: " << error_count << std::endl;

  if (result.count("summary")) {
    std::cout << "Summary written to: " << result["summary"].as<std::string>()
              << std::endl;
  }

  return error_count > 0 ? 1 : 0;
}

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 1;

  if (argc < 2) {
    PrintUsage();
    return 1;
  }

  std::string command = argv[1];

  if (command == "help" || command == "--help" || command == "-h") {
    PrintUsage();
    return 0;
  } else if (command == "analyze") {
    return CmdAnalyze(argc - 1, argv + 1);
  } else if (command == "batch") {
    return CmdBatch(argc - 1, argv + 1);
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    std::cerr << "Run 'analyzer_cli help' for usage." << std::endl;
    return 1;
  }
}
