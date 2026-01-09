#include "analyzer/simulator.h"
#include "stats/player_stats.h"
#include "stats/player_stats_config.h"
#include "mahjong/handtiles.h"
#include "mahjong/tile.h"
#include <gtest/gtest.h>
#include <glog/logging.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using json   = nlohmann::json;
namespace fs = std::filesystem;

namespace tziakcha {
namespace stats {

class PlayerStatsTest : public ::testing::Test {
protected:
  std::string temp_dir_ = "test_data_player_stats";

  void SetUp() override {
    // Create temp directory for records
    if (fs::exists(temp_dir_)) {
      fs::remove_all(temp_dir_);
    }
    fs::create_directories(temp_dir_);
  }

  void TearDown() override {
    // Clean up
    if (fs::exists(temp_dir_)) {
      fs::remove_all(temp_dir_);
    }
  }

  void CreateRecord(const std::string& filename, const json& content) {
    std::ofstream ofs(temp_dir_ + "/" + filename);
    ofs << content.dump();
  }
};

TEST_F(PlayerStatsTest, CalculatesBasicStatsAndShanten) {
  // Sanity check: Can we use GB-Mahjong?
  {
    mahjong::Handtiles hand;
    LOG(INFO) << "Sanity check: Adding Tile 1";
    hand.lipai.push_back(mahjong::Tile(1));
    hand.lipai_table[1]++;
    LOG(INFO) << "Sanity check: Success";
  }

  // 1. Prepare a synthetic record
  // ... (construct wall)
  std::string wall_hex;
  auto hex_byte = [](int val) {
    char buf[3];
    sprintf(buf, "%02x", val);
    return std::string(buf);
  };

  std::vector<int> wall_data(144, 136); // Initialize with flowers skip
  // P0 Winner (Dealer): Dice=17, start_pos=112.
  // P0 Hand tiles come from wall indices:
  // (112+0..3)%144 = 112, 113, 114, 115
  // (112+16..19)%144 = 128, 129, 130, 131
  // (112+32..35)%144 = 0, 1, 2, 3
  // (112+48)%144 = 16
  // (112+52)%144 = 20

  // Hand: 111m, 222m, 333m, 444m, 55m (Winning)
  wall_data[112] = 0;
  wall_data[113] = 1;
  wall_data[114] = 2; // 111m
  wall_data[115] = 4;
  wall_data[128] = 5;
  wall_data[129] = 6; // 222m
  wall_data[130] = 8;
  wall_data[131] = 9;
  wall_data[0]   = 10; // 333m
  wall_data[1]   = 12;
  wall_data[2]   = 13;
  wall_data[3]   = 14; // 444m
  wall_data[16]  = 16;
  wall_data[20]  = 17; // 55m

  // Fill the rest with distinct tiles to avoid count > 4
  std::vector<int> used_counts(35, 0);
  for (int idx : {112, 113, 114, 115, 128, 129, 130, 131, 0, 1, 2, 3, 16, 20}) {
    used_counts[wall_data[idx] >> 2]++;
  }

  for (int i = 0; i < 144; ++i) {
    if (i == 112 || i == 113 || i == 114 || i == 115 || i == 128 || i == 129 ||
        i == 130 || i == 131 || i == 0 || i == 1 || i == 2 || i == 3 ||
        i == 16 || i == 20)
      continue;

    // Find a tile with count < 4
    for (int t = 20; t < 34; ++t) { // Start from index 20 (Winds/Dragons)
      if (used_counts[t] < 4) {
        wall_data[i] = t * 4 + used_counts[t];
        used_counts[t]++;
        break;
      }
    }
  }

  for (int i = 0; i < 144; ++i) {
    wall_hex += hex_byte(wall_data[i]);
  }

  json actions = json::array();
  // Action 0: Type 0 (Start Play) - captures starting hand
  // P0 starts.
  actions.push_back({0, 0, 0});

  // Let P0 win immediately (Tenhou?) or just end game.
  // Actually, to trigger "win_count" stats, we need a WIN action.
  // Action: P0 Tsumo (Type 6, data=fan<<1 | 1 (tsumo=false? wait))
  // Type 6: Win. Data logic:
  // bit 0: is_auto (1=auto, 0=manual) - logic in player_stats.cpp says is_auto
  // affects duration not win type? Actually Win detection in player_stats:
  // "flag_info.winners" from script["g"]["w"] or similar?
  // Let's look at `ParseWinFlags`: script["g"]["w"] (integer flags).
  // 1<<0 = P0 win.
  // 1<<4 = P0 discarder.

  // We need to set up `script` correctly.

  json script;
  script["w"] = wall_hex;
  script["a"] = actions;

  // Game config / results
  // w: win flags.
  // If P0 self-draw wins: winners=[0], discarder=0.
  // 1<<0 | 1<<4 = 1 | 16 = 17.
  script["g"]["w"] = 17;

  // Players
  json players = json::array();
  for (int i = 0; i < 4; ++i) {
    players.push_back(
        {{"i", "p" + std::to_string(i)}, {"n", "Player" + std::to_string(i)}});
  }
  script["p"] = players;

  // Wins array (needed for fan info)
  json wins = json::array();
  // P0 win data
  wins.push_back(
      {{"fan", 88},
       {"fans",
        json::array(
            {{{"name", "Big Four Winds"}, {"points", 88}, {"count", 1}}})}});
  // Others empty
  wins.push_back({});
  wins.push_back({});
  wins.push_back({});

  script["y"] = wins; // "y" is usually matches wins in standard format?
  // player_stats checks script["wins"]?
  // Code says `player_stats.cpp`:
  // `record.script_json.contains(PlayerStatsConfig::kScriptWins)` -> "y"?
  // `PlayerStatsConfig::kScriptWins` usually maps to "y" in minified json.
  // Let's check player_stats_config.h if we could... but conventionally "y".
  // Actually `ParseWinFlags` uses `PlayerStatsConfig::kScriptWinFlags` -> "v"?
  // or "g"["w"]? Let's verify with example_record.json content if possible, or
  // guess. example_record.json: "y" is array. "v" is 0. "step" has "g":
  // {"w":...}? No, "g" is game config. Wait, `ParseWinFlags` reads
  // `script_json.value(kScriptWinFlags, 0)`. If kScriptWinFlags is "v", then
  // script["v"].

  script["y"] = wins;
  script["b"] = 17; // Correct WinFlags key is "b"
  script["d"] = 17; // Dice data: val&15=1, (val>>4)&15=1 -> 1,1

  json record;
  record["step"] = script;
  record["id"]   = "test_rec_1";
  record["t"]    = 1000000; // timestamp

  CreateRecord("test_1.json", record);

  // 2. Run Player Stats
  PlayerStatsOptions options;
  options.record_dir = temp_dir_;
  options.output_dir = temp_dir_ + "/out";
  options.limit      = 0;

  ASSERT_TRUE(RunPlayerStats(options));

  // 3. Verify Output
  // Check P0 stats
  fs::path p0_path = fs::path(options.output_dir) / "p0.json";
  ASSERT_TRUE(fs::exists(p0_path));

  std::ifstream ifs(p0_path);
  json j;
  ifs >> j;

  // Basic Stats
  EXPECT_EQ(j["stats"]["win_count"], 1);
  EXPECT_EQ(j["stats"]["tsumo_win_count"], 1);
  EXPECT_EQ(j["stats"]["total_rounds"], 1);

  // Shanten Stats
  // P0 hand was all 1s (13 or 14 tiles).
  // 14 tiles of 1m: 1111 1111 1111 11.

  // Aliases check
  EXPECT_TRUE(j.contains("aliases"));
  EXPECT_EQ(j["aliases"].size(), 1);
  // Order in JSON array might vary if from hash set, but size 1 is
  // deterministic
  EXPECT_EQ(j["aliases"][0], "Player0");

  // Processed records removal check
  EXPECT_FALSE(j.contains("processed_records"));
  // This is technically invalid (max 4 per tile), but implementation doesn't
  // check count validity for tile existence. Shanten calculator should return
  // something. 14 tiles of same tile -> Quad? Standard shanten logic: 4 sets +
  // 1 pair. 1m x 14. 111, 111, 111, 111, 11. (4 sets + pair). This is a winning
  // hand (Chuuren Poutou? No, 11111.. is just sets). Win = -1 shanten.

  // Check if starting shanten statistic is present
  EXPECT_TRUE(j["stats"].contains("avg_starting_shanten_14"));
  EXPECT_TRUE(j["stats"].contains("starting_shanten_count_14"));

  EXPECT_EQ(j["stats"]["starting_shanten_count_14"], 1);
  // avg shanten should be -1.0
  EXPECT_DOUBLE_EQ(j["stats"]["avg_starting_shanten_14"], -1.0);

  // 13-tile stats should be 0
  EXPECT_EQ(j["stats"].value("starting_shanten_count_13", 0), 0);
}

TEST_F(PlayerStatsTest, CalculatesRonAndDealInStats) {
  // P0 wins by Ron from P1
  // winners_mask = 1 << 0 = 1
  // discarder_mask = 1 << 1 = 2
  // WinFlags = (2 << 4) | 1 = 33
  int win_flags = 33;

  json script;
  script["a"]      = json::array({{0, 0, 0}});
  script["g"]["w"] = win_flags;
  script["b"]      = win_flags;
  script["d"]      = 17;

  json players = json::array();
  for (int i = 0; i < 4; ++i) {
    players.push_back(
        {{"i", "p" + std::to_string(i)}, {"n", "Player" + std::to_string(i)}});
  }
  script["p"] = players;

  // Empty wall just to satisfy parser
  std::string wall_hex(288, '0');
  script["w"] = wall_hex;

  json record;
  record["step"] = script;
  record["id"]   = "test_ron_1";
  record["t"]    = 2000000;

  CreateRecord("test_ron.json", record);

  PlayerStatsOptions options;
  options.record_dir = temp_dir_;
  options.output_dir = temp_dir_ + "/out_ron";
  options.limit      = 0;

  ASSERT_TRUE(RunPlayerStats(options));

  // Verify P0 (Winner)
  {
    std::ifstream ifs(fs::path(options.output_dir) / "p0.json");
    json j;
    ifs >> j;
    EXPECT_EQ(j["stats"]["win_count"], 1);
    EXPECT_EQ(j["stats"]["tsumo_win_count"], 0); // Ron, not Tsumo
  }

  // Verify P1 (Discarder)
  {
    std::ifstream ifs(fs::path(options.output_dir) / "p1.json");
    json j;
    ifs >> j;
    EXPECT_EQ(j["stats"]["deal_in_count"], 1);
    EXPECT_EQ(j["stats"]["win_count"], 0);
  }
}

TEST_F(PlayerStatsTest, CalculatesDrawStats) {
  // win_flags = 0 -> Draw
  int win_flags = 0;

  json script;
  script["a"]      = json::array({{0, 0, 0}});
  script["g"]["w"] = win_flags;
  script["b"]      = win_flags;
  script["d"]      = 17;

  json players = json::array();
  for (int i = 0; i < 4; ++i) {
    players.push_back(
        {{"i", "p" + std::to_string(i)}, {"n", "Player" + std::to_string(i)}});
  }
  script["p"] = players;
  script["w"] = std::string(288, '0');

  json record;
  record["step"] = script;
  record["id"]   = "test_draw_1";
  record["t"]    = 3000000;

  CreateRecord("test_draw.json", record);

  PlayerStatsOptions options;
  options.record_dir = temp_dir_;
  options.output_dir = temp_dir_ + "/out_draw";
  options.limit      = 0;

  ASSERT_TRUE(RunPlayerStats(options));

  // All players should have 1 draw
  for (int i = 0; i < 4; ++i) {
    std::ifstream ifs(
        fs::path(options.output_dir) / ("p" + std::to_string(i) + ".json"));
    json j;
    ifs >> j;
    EXPECT_EQ(j["stats"]["draw_count"], 1);
    EXPECT_EQ(j["stats"]["total_rounds"], 1);
  }
}

TEST_F(PlayerStatsTest, CalculatesSessionDurationAndSteps) {
  json actions = json::array();
  // Action encoding: (player_idx << 4) | action_type
  // t=0: Start (P0, type 0)
  actions.push_back({(0 << 4) | 0, 0, 0});
  // t=1000: Action 1 (P1, type 2 Discard)
  actions.push_back({(1 << 4) | 2, 1, 1000});
  // t=3000: Action 2 (P2, type 3 CHI)
  actions.push_back({(2 << 4) | 3, 2, 3000});

  // Total duration = 3000ms = 3s.
  // Total steps = 3.

  json script;
  script["a"] = actions;
  script["b"] = 0; // Draw
  script["d"] = 17;
  script["p"] = json::array();
  for (int i = 0; i < 4; ++i) {
    script["p"].push_back(
        {{"i", "p" + std::to_string(i)}, {"n", "P" + std::to_string(i)}});
  }
  script["w"] = std::string(288, '0');

  json record;
  record["step"] = script;
  record["id"]   = "test_session_1";
  record["t"]    = 4000000;

  CreateRecord("test_session.json", record);

  PlayerStatsOptions options;
  options.record_dir = temp_dir_;
  options.output_dir = temp_dir_ + "/out_session";
  options.limit      = 0;

  ASSERT_TRUE(RunPlayerStats(options));

  // P1 should have 1s and 1 step
  {
    std::ifstream ifs(fs::path(options.output_dir) / "p1.json");
    json j;
    ifs >> j;
    EXPECT_DOUBLE_EQ(j["stats"]["total_session_seconds"], 1.0);
    EXPECT_EQ(j["stats"]["total_steps"], 1);
  }

  // P2 should have 2s and 1 step
  {
    std::ifstream ifs(fs::path(options.output_dir) / "p2.json");
    json j;
    ifs >> j;
    EXPECT_DOUBLE_EQ(j["stats"]["total_session_seconds"], 2.0);
    EXPECT_EQ(j["stats"]["total_steps"], 1);
  }
}

} // namespace stats
} // namespace tziakcha
