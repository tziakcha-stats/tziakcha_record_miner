#include "calc/shanten_calculator.h"
#include "calc/shanten_algo.h"
#include "utils/shanten_utils.h"

namespace calc {

ShantenCalculator::ShantenCalculator() {}

ShantenCalculator::~ShantenCalculator() {}

ShantenResult ShantenCalculator::Calculate(const mahjong::Handtiles& hand) {
  ShantenResult result;

  std::vector<int> tiles = tziakcha::utils::ConvertHandToAlgo(hand);

  int melds_count = hand.fulu.size();

  result.standard = ShantenAlgo::CalculateStandard(tiles, melds_count);

  if (melds_count == 0) {
    result.seven_pairs = ShantenAlgo::CalculateSevenPairs(tiles);
  } else {
    result.seven_pairs = 99;
  }

  if (melds_count == 0) {
    result.thirteen_orphans = ShantenAlgo::CalculateThirteenOrphans(tiles);
  } else {
    result.thirteen_orphans = 99;
  }

  if (melds_count == 0) {
    result.all_unrelated = ShantenAlgo::CalculateAllUnrelated(tiles);
  } else {
    result.all_unrelated = 99;
  }

  result.knitted_dragon =
      ShantenAlgo::CalculateKnittedDragon(tiles, melds_count);

  return result;
}

void ShantenCalculator::CalculateKnittedDragonDetail(
    const mahjong::Handtiles& hand, ShantenResult& result) {
  std::vector<int> tiles = tziakcha::utils::ConvertHandToAlgo(hand);
  int melds              = hand.fulu.size();

  for (int i = 0; i < 34; ++i) {
    if (tiles[i] > 0) {
      tiles[i]--;
      std::vector<std::string> waits;
      for (int k = 0; k < 34; ++k) {
        if (tiles[k] < 4) {
          tiles[k]++;
          int new_shanten = ShantenAlgo::CalculateKnittedDragon(tiles, melds);
          if (new_shanten == -1) {
            waits.push_back(tziakcha::utils::GetAlgoTileName(k));
          }
          tiles[k]--;
        }
      }

      if (!waits.empty()) {
        ShantenDetail detail;
        detail.discard_tile  = tziakcha::utils::GetAlgoTileName(i);
        detail.waiting_tiles = waits;
        result.knitted_dragon_details.push_back(detail);
      }

      tiles[i]++;
    }
  }
}

} // namespace calc
