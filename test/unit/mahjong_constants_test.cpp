#include <gtest/gtest.h>
#include "base/mahjong_constants.h"

using namespace tziakcha::base;

TEST(MahjongConstantsTest, WindConstants) {
  EXPECT_EQ(WIND.size(), 4);
  EXPECT_EQ(WIND[0], "东");
  EXPECT_EQ(WIND[1], "南");
  EXPECT_EQ(WIND[2], "西");
  EXPECT_EQ(WIND[3], "北");
}

TEST(MahjongConstantsTest, TileIdentity) {
  EXPECT_EQ(TILE_IDENTITY.size(), 34);
  EXPECT_EQ(TILE_IDENTITY[0], "1m");
  EXPECT_EQ(TILE_IDENTITY[8], "9m");
  EXPECT_EQ(TILE_IDENTITY[9], "1s");
  EXPECT_EQ(TILE_IDENTITY[17], "9s");
  EXPECT_EQ(TILE_IDENTITY[18], "1p");
  EXPECT_EQ(TILE_IDENTITY[26], "9p");
  EXPECT_EQ(TILE_IDENTITY[27], "E");
  EXPECT_EQ(TILE_IDENTITY[28], "S");
  EXPECT_EQ(TILE_IDENTITY[29], "W");
  EXPECT_EQ(TILE_IDENTITY[30], "N");
  EXPECT_EQ(TILE_IDENTITY[31], "C");
  EXPECT_EQ(TILE_IDENTITY[32], "F");
  EXPECT_EQ(TILE_IDENTITY[33], "B");
}

TEST(MahjongConstantsTest, FlowerTiles) {
  EXPECT_EQ(FLOWER_TILES.size(), 8);
  EXPECT_EQ(FLOWER_TILES[0], "1f");
  EXPECT_EQ(FLOWER_TILES[7], "8f");
}

TEST(MahjongConstantsTest, PackActionMap) {
  EXPECT_EQ(PACK_ACTION_MAP.size(), 3);
  EXPECT_EQ(PACK_ACTION_MAP.at(3), "CHI");
  EXPECT_EQ(PACK_ACTION_MAP.at(4), "PENG");
  EXPECT_EQ(PACK_ACTION_MAP.at(5), "GANG");
}

TEST(MahjongConstantsTest, S2OMapping) {
  EXPECT_EQ(S2O.size(), 16);
  EXPECT_EQ(S2O[0][0], 0);
  EXPECT_EQ(S2O[0][1], 1);
  EXPECT_EQ(S2O[0][2], 2);
  EXPECT_EQ(S2O[0][3], 3);
  EXPECT_EQ(S2O[1][0], 1);
  EXPECT_EQ(S2O[1][3], 0);
}

TEST(MahjongConstantsTest, FanNames) {
  EXPECT_EQ(FAN_NAMES.size(), 89);
  EXPECT_EQ(FAN_NAMES[0], "无");
  EXPECT_EQ(FAN_NAMES[1], "大四喜");
  EXPECT_EQ(FAN_NAMES[2], "大三元");
  EXPECT_EQ(FAN_NAMES[3], "绿一色");
  EXPECT_EQ(FAN_NAMES[80], "独听・嵌张");
}
