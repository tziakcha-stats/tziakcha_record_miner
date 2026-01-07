#ifndef SHANTEN_ALGO_H
#define SHANTEN_ALGO_H

#include <vector>
#include <algorithm>

namespace calc {

class ShantenAlgo {
public:
  static int CalculateStandard(const std::vector<int>& tiles, int melds_count);

  static int CalculateSevenPairs(const std::vector<int>& tiles);

  static int CalculateThirteenOrphans(const std::vector<int>& tiles);

  static int CalculateAllUnrelated(const std::vector<int>& tiles);

  static int
  CalculateKnittedDragon(const std::vector<int>& tiles, int melds_count);

private:
  static int
  RunStandard(std::vector<int>& tiles, int sets_needed, int pairs_needed);
};

} // namespace calc

#endif // SHANTEN_ALGO_H
