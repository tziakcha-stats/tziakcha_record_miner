#include <gtest/gtest.h>
#include "calc/fan_calculator.h"
#include "calc/shanten_calculator.h"
#include <string>
#include <vector>

mahjong::Handtiles CreateHand(const std::string& handtiles_str) {
  mahjong::Handtiles hand;
  if (hand.StringToHandtiles(handtiles_str) != 0) {
  }
  return hand;
}

class ShantenTest : public ::testing::Test {
protected:
  calc::ShantenCalculator calculator;
};

TEST_F(ShantenTest, Example1) {
  auto hand   = CreateHand("1259m2356p55679sF");
  auto result = calculator.Calculate(hand);

  EXPECT_EQ(result.standard, 3);
  EXPECT_EQ(result.seven_pairs, 5);
  EXPECT_EQ(result.thirteen_orphans, 9);
  EXPECT_EQ(result.all_unrelated, 7);
  EXPECT_EQ(result.knitted_dragon, 5);
}

TEST_F(ShantenTest, Example2) {
  auto hand   = CreateHand("2369m458p68sEESSC");
  auto result = calculator.Calculate(hand);

  EXPECT_EQ(result.standard, 3);
  EXPECT_EQ(result.seven_pairs, 4);
  EXPECT_EQ(result.thirteen_orphans, 8);
  EXPECT_EQ(result.all_unrelated, 5);
  EXPECT_EQ(result.knitted_dragon, 4);
}

TEST_F(ShantenTest, Example3) {
  auto hand   = CreateHand("345m33345p2468sEP");
  auto result = calculator.Calculate(hand);

  EXPECT_EQ(result.standard, 1);
  EXPECT_EQ(result.seven_pairs, 5);
  EXPECT_EQ(result.thirteen_orphans, 11);
  EXPECT_EQ(result.all_unrelated, 7);
  EXPECT_EQ(result.knitted_dragon, 5);
}

TEST_F(ShantenTest, Example4) {
  auto hand   = CreateHand("28m1268p134578sEW");
  auto result = calculator.Calculate(hand);

  EXPECT_EQ(result.standard, 3);
  EXPECT_EQ(result.seven_pairs, 6);
  EXPECT_EQ(result.thirteen_orphans, 9);
  EXPECT_EQ(result.all_unrelated, 5);
  EXPECT_EQ(result.knitted_dragon, 4);
}

TEST_F(ShantenTest, Example5_Winning) {
  auto hand   = CreateHand("11233345p45677s2p");
  auto result = calculator.Calculate(hand);

  EXPECT_EQ(result.standard, -1);
  EXPECT_EQ(result.seven_pairs, 2);
  EXPECT_EQ(result.thirteen_orphans, 11);
  EXPECT_EQ(result.all_unrelated, 9);
  EXPECT_EQ(result.knitted_dragon, 4);
}

TEST_F(ShantenTest, Example13Tile_1) {
  auto hand   = CreateHand("1148m16p122468sW");
  auto result = calculator.Calculate(hand);

  EXPECT_EQ(result.standard, 4);
  EXPECT_EQ(result.seven_pairs, 4);
  EXPECT_EQ(result.thirteen_orphans, 8);
  EXPECT_EQ(result.all_unrelated, 7);
  EXPECT_EQ(result.knitted_dragon, 5);
}

TEST_F(ShantenTest, Example13Tile_2) {
  auto hand   = CreateHand("5678m4p566sSSPPP");
  auto result = calculator.Calculate(hand);

  EXPECT_EQ(result.standard, 2);
  EXPECT_EQ(result.seven_pairs, 3);
  EXPECT_EQ(result.thirteen_orphans, 10);
  EXPECT_EQ(result.all_unrelated, 7);
  EXPECT_EQ(result.knitted_dragon, 4);
}

TEST_F(ShantenTest, Example13Tile_3_Tenpai) {
  auto hand   = CreateHand("567m45p666sSSPPP");
  auto result = calculator.Calculate(hand);

  EXPECT_EQ(result.standard, 0);
  EXPECT_EQ(result.seven_pairs, 3);
  EXPECT_EQ(result.thirteen_orphans, 10);
  EXPECT_EQ(result.all_unrelated, 8);
  EXPECT_EQ(result.knitted_dragon, 5);
}
