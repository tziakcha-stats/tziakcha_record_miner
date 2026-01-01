#ifndef FAN_CALCULATOR_H
#define FAN_CALCULATOR_H

#include <string>
#include <vector>
#include "fan.h"
#include "handtiles.h"

namespace calc {

struct FanResult {
    std::string fan_name;
    int fan_score;
    std::vector<std::string> pack_descriptions;
};

struct FanTypeInfo {
    mahjong::fan_t fan_type;
    std::string fan_name;
    int count;
    int score_per_fan;
    int total_score;
};

class FanCalculator {
public:
    FanCalculator();
    ~FanCalculator();

    bool ParseHandtiles(const std::string& handtiles_str);

    bool IsWinningHand() const;

    bool CalculateFan();

    int GetTotalFan() const;

    std::string GetStandardHandtilesString() const;

    std::vector<FanResult> GetFanDetails() const;

    std::vector<FanTypeInfo> GetFanTypesSummary() const;

    int GetFanTypeCount(mahjong::fan_t fan_type) const;

    bool HasFanType(mahjong::fan_t fan_type) const;

    std::vector<mahjong::fan_t> GetAllFanTypes() const;

private:
    mahjong::Handtiles handtiles_;
    mahjong::Fan fan_;
    bool is_parsed_;
    bool is_calculated_;
};

} // namespace calc

#endif // FAN_CALCULATOR_H
