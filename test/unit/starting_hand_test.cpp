#include "analyzer/simulator.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace tziakcha {
namespace analyzer {

class StartingHandTest : public ::testing::Test {
protected:
  void SetUp() override {}
};

TEST_F(StartingHandTest, CapturesStartingHandFromExampleRecord) {
  // Use absolute path as requested for user environment
  std::string file_path =
      "/Users/choimoe/Code/tziakcha_record_miner/test/unit/"
      "assets/example_record.json";
  std::ifstream ifs(file_path);
  ASSERT_TRUE(ifs.is_open()) << "Failed to open " << file_path;

  json record;
  try {
    ifs >> record;
  } catch (const std::exception& e) {
    FAIL() << "Failed to parse json: " << e.what();
  }

  // The provided example_record.json has "privacy info hidden", which likely
  // includes the wall. RecordSimulator requires a wall ("w") to deal initial
  // hands. We inject a dummy wall (all 1m) to allow the simulation to proceed
  // through the Deal phase and reach the "Start Play" (Type 0) action where we
  // capture the hands.
  if (record.contains("step")) {
    if (!record["step"].contains("w")) {
      std::string w_hex;
      // 144 tiles, represented as hex bytes (0x01 = 1 Man)
      for (int i = 0; i < 144; ++i)
        w_hex += "01";
      record["step"]["w"] = w_hex;
    }
  } else {
    FAIL() << "Record missing 'step' field";
  }

  RecordSimulator simulator;
  auto result = simulator.Simulate(record.dump());

  // Verify that starting hands were captured for 4 players
  ASSERT_EQ(result.starting_hands.size(), 4)
      << "Should capture hands for 4 players";

  for (int i = 0; i < 4; ++i) {
    const auto& hand = result.starting_hands[i];
    EXPECT_GE(hand.size(), 13) << "Player " << i << " hand size too small";
  }
}

} // namespace analyzer
} // namespace tziakcha
