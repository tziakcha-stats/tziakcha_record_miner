#ifndef SHANTEN_CALCULATOR_H
#define SHANTEN_CALCULATOR_H

#include "base/shanten_result.h"
#include "handtiles.h"
#include <string>

namespace calc {

class ShantenCalculator {
public:
  ShantenCalculator();
  ~ShantenCalculator();

  ShantenResult Calculate(const mahjong::Handtiles& hand);

  std::vector<std::string> GetWaitingTiles(const mahjong::Handtiles& hand);

  void CalculateKnittedDragonDetail(const mahjong::Handtiles& hand,
                                    ShantenResult& result);

  void AnalyzeWait(const mahjong::Handtiles& hand, ShantenResult& result);
};

} // namespace calc

#endif // SHANTEN_CALCULATOR_H
