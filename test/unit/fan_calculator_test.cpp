#include <gtest/gtest.h>
#include <glog/logging.h>
#include "calc/fan_calculator.h"

class FanCalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        calculator = std::make_unique<calc::FanCalculator>();
    }

    void TearDown() override {
        calculator.reset();
    }

    std::unique_ptr<calc::FanCalculator> calculator;
};

TEST_F(FanCalculatorTest, ParseValidHandtiles) {
    EXPECT_TRUE(calculator->ParseHandtiles("123789s123789p33m"));
    EXPECT_FALSE(calculator->GetStandardHandtilesString().empty());
}

TEST_F(FanCalculatorTest, ParseEmptyString) {
    calculator->ParseHandtiles("");
    SUCCEED();
}

TEST_F(FanCalculatorTest, IsWinningHandPinghu) {
    ASSERT_TRUE(calculator->ParseHandtiles("123789s123789p33m"));
    EXPECT_TRUE(calculator->IsWinningHand());
}

TEST_F(FanCalculatorTest, NotWinningHand) {
    ASSERT_TRUE(calculator->ParseHandtiles("123456789m"));
    EXPECT_FALSE(calculator->IsWinningHand());
}

TEST_F(FanCalculatorTest, IsWinningHandBeforeParse) {
    EXPECT_FALSE(calculator->IsWinningHand());
}

TEST_F(FanCalculatorTest, PinghuCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("12345566789s22p4s|EE1000"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GT(calculator->GetTotalFan(), 0);
    
    auto details = calculator->GetFanDetails();
    EXPECT_FALSE(details.empty());
}

TEST_F(FanCalculatorTest, QiduiCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("556699m22334455p"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GE(calculator->GetTotalFan(), 24);
}

TEST_F(FanCalculatorTest, QingyiseCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("[123s,1][333s,2]45678996s|EE1000"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GE(calculator->GetTotalFan(), 24);
}

TEST_F(FanCalculatorTest, HunyiseCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("[123m,1][345m,1]67789mCC5m|EE1000"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GT(calculator->GetTotalFan(), 0);
}

TEST_F(FanCalculatorTest, PengpenghuCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("[234m,1][555m,1]567m55566p"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GT(calculator->GetTotalFan(), 0);
}

TEST_F(FanCalculatorTest, XiaoyuwuCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("[123p,3]23334p222444s|EE1000"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GE(calculator->GetTotalFan(), 12);
}

TEST_F(FanCalculatorTest, YibangaoCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("[234m,1]11234p233442s"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GT(calculator->GetTotalFan(), 0);
}

TEST_F(FanCalculatorTest, XixiangfengCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("12345789p55678m6p|EE1000"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GT(calculator->GetTotalFan(), 0);
}

TEST_F(FanCalculatorTest, CalculateFanBeforeParse) {
    EXPECT_FALSE(calculator->CalculateFan());
}

TEST_F(FanCalculatorTest, CalculateFanNotWinning) {
    ASSERT_TRUE(calculator->ParseHandtiles("123456789m"));
    EXPECT_FALSE(calculator->CalculateFan());
}

TEST_F(FanCalculatorTest, GetTotalFanBeforeCalculate) {
    EXPECT_EQ(calculator->GetTotalFan(), 0);
}

TEST_F(FanCalculatorTest, GetFanDetailsBeforeCalculate) {
    auto details = calculator->GetFanDetails();
    EXPECT_TRUE(details.empty());
}

TEST_F(FanCalculatorTest, GetStandardHandtilesStringBeforeParse) {
    EXPECT_TRUE(calculator->GetStandardHandtilesString().empty());
}

TEST_F(FanCalculatorTest, MultipleCalculations) {
    ASSERT_TRUE(calculator->ParseHandtiles("123789s123789p33m"));
    ASSERT_TRUE(calculator->CalculateFan());
    int total_fan_1 = calculator->GetTotalFan();
    
    ASSERT_TRUE(calculator->CalculateFan());
    int total_fan_2 = calculator->GetTotalFan();
    
    EXPECT_EQ(total_fan_1, total_fan_2);
}

TEST_F(FanCalculatorTest, ReparseHandtiles) {
    ASSERT_TRUE(calculator->ParseHandtiles("123789s123789p33m"));
    ASSERT_TRUE(calculator->CalculateFan());
    int total_fan_1 = calculator->GetTotalFan();
    
    ASSERT_TRUE(calculator->ParseHandtiles("556699m22334455p"));
    ASSERT_TRUE(calculator->CalculateFan());
    int total_fan_2 = calculator->GetTotalFan();
    
    EXPECT_NE(total_fan_1, total_fan_2);
}

TEST_F(FanCalculatorTest, HandtilesWithFlowers) {
    ASSERT_TRUE(calculator->ParseHandtiles("123789s123789p33m|EE0000|abcd"));
    EXPECT_TRUE(calculator->IsWinningHand());
}

TEST_F(FanCalculatorTest, HandtilesWithSituation) {
    ASSERT_TRUE(calculator->ParseHandtiles("123789s123789p33m|EE1000"));
    EXPECT_TRUE(calculator->IsWinningHand());
}

TEST_F(FanCalculatorTest, MeldsCase) {
    ASSERT_TRUE(calculator->ParseHandtiles("[345s,2]34555567p[789m]"));
    ASSERT_TRUE(calculator->CalculateFan());
    EXPECT_GT(calculator->GetTotalFan(), 0);
}

TEST_F(FanCalculatorTest, FanResultStructure) {
    ASSERT_TRUE(calculator->ParseHandtiles("12345566789s22p4s|EE1000"));
    ASSERT_TRUE(calculator->CalculateFan());
    
    auto details = calculator->GetFanDetails();
    ASSERT_FALSE(details.empty());
    
    for (const auto& detail : details) {
        EXPECT_FALSE(detail.fan_name.empty());
        EXPECT_GT(detail.fan_score, 0);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 0;
    
    return RUN_ALL_TESTS();
}
