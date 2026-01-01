#include <iostream>
#include <string>
#include <glog/logging.h>
#include <cxxopts.hpp>
#include "calc/fan_calculator.h"

void PrintUsageExamples() {
  std::cout << "Examples:\n";
  std::cout << "  1. Simple winning hand:\n";
  std::cout << "     fan_calculator \"[123m,1][123p,1]123m12p44s3p\"\n\n";
  std::cout << "  2. With flowers and situation:\n";
  std::cout << "     fan_calculator \"11123456789999m|EE1000|cbaghdfe\"\n\n";
  std::cout << "  3. With verbose logging:\n";
  std::cout << "     fan_calculator \"123789s123789p33m\" --verbose\n";
}

void PrintHandtilesFormat() {
  std::cout << "\nHandtiles string format:\n";
  std::cout << "  Basic tiles: [1-9]+[msp] for number tiles, [ESWNCFP] for "
               "honor tiles\n";
  std::cout << "  Melds: [XXX,N] for melded tiles (pung/kong/chow)\n";
  std::cout << "  Situation: |WW0000 format (wind|self-drawn|absolute "
               "terminal|sea bottom|kong rob)\n";
  std::cout << "  Flowers: flowers count or names like |fah\n";
}

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);

  cxxopts::Options options(
      "fan_calculator", "Mahjong Fan Calculator - GB Mahjong");

  options.add_options()("h,help", "Print help information")(
      "v,verbose", "Enable verbose logging output")(
      "handtiles", "Handtiles string", cxxopts::value<std::string>())(
      "example", "Show usage examples");

  options.parse_positional({"handtiles"});
  options.positional_help("<handtiles_string>");

  try {
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help();
      PrintHandtilesFormat();
      return 0;
    }

    if (result.count("example")) {
      PrintUsageExamples();
      return 0;
    }

    if (!result.count("handtiles")) {
      std::cerr << "Error: handtiles string is required\n";
      std::cout << "\n" << options.help();
      return 1;
    }

    std::string handtiles_str = result["handtiles"].as<std::string>();

    if (result.count("verbose")) {
      FLAGS_logtostderr = 1;
      FLAGS_v           = 1;
    }

    LOG(INFO) << "Starting fan calculation for handtiles: " << handtiles_str;

    calc::FanCalculator calculator;

    if (!calculator.ParseHandtiles(handtiles_str)) {
      LOG(ERROR) << "Failed to parse handtiles string";
      std::cerr << "Error: Invalid handtiles string\n";
      return 1;
    }

    LOG(INFO) << "Parsed handtiles: "
              << calculator.GetStandardHandtilesString();

    if (!calculator.IsWinningHand()) {
      LOG(WARNING) << "Not a winning hand";
      std::cout << "Not a winning hand\n";
      return 1;
    }

    LOG(INFO) << "Confirmed winning hand, proceeding with fan calculation";

    if (!calculator.CalculateFan()) {
      LOG(ERROR) << "Fan calculation failed";
      std::cerr << "Error: Fan calculation failed\n";
      return 1;
    }

    std::cout << "Handtiles: " << calculator.GetStandardHandtilesString()
              << "\n";
    std::cout << "Total Fan: " << calculator.GetTotalFan() << "\n\n";

    auto fan_details = calculator.GetFanDetails();

    if (fan_details.empty()) {
      LOG(WARNING) << "No fan patterns found";
      std::cout << "No fan patterns found\n";
      return 1;
    }

    std::cout << "Fan Details:\n";
    for (size_t i = 0; i < fan_details.size(); ++i) {
      const auto& detail = fan_details[i];
      std::cout << "  " << (i + 1) << ". " << detail.fan_name << " ("
                << detail.fan_score << " fan)";

      if (!detail.pack_descriptions.empty()) {
        LOG(INFO) << "  Fan " << (i + 1) << ": " << detail.fan_name << " ("
                  << detail.fan_score
                  << " fan) - Packs: " << detail.pack_descriptions.size();
        for (const auto& pack : detail.pack_descriptions) {
          LOG(INFO) << "    Pack: " << pack;
        }
      }

      std::cout << "\n";
    }

    LOG(INFO) << "Fan calculation completed successfully. Total: "
              << calculator.GetTotalFan() << " fan, " << fan_details.size()
              << " pattern(s)";

    return 0;

  } catch (const cxxopts::exceptions::exception& e) {
    LOG(ERROR) << "Command line parsing error: " << e.what();
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception occurred: " << e.what();
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  } catch (...) {
    LOG(ERROR) << "Unknown exception occurred";
    std::cerr << "Error: Unknown error occurred\n";
    return 1;
  }
}
