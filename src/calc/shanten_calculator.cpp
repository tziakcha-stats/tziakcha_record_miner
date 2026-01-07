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

  AnalyzeWait(hand, result);

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

void ShantenCalculator::AnalyzeWait(const mahjong::Handtiles& hand,
                                    ShantenResult& result) {
  if (result.standard != 1) {
    return;
  }

  std::vector<int> original_tiles = tziakcha::utils::ConvertHandToAlgo(hand);
  int melds_count                 = hand.fulu.size();

  for (int i = 0; i < 34; ++i) {
    if (original_tiles[i] > 0) {
      original_tiles[i]--;

      ShantenDetail detail;
      detail.discard_tile = tziakcha::utils::GetAlgoTileName(i);

      for (int k = 0; k < 34; ++k) {
        if (original_tiles[k] < 4) {
          original_tiles[k]++;

          int new_shanten =
              ShantenAlgo::CalculateStandard(original_tiles, melds_count);
          if (new_shanten == 0) {
            std::string wait_tile_name = tziakcha::utils::GetAlgoTileName(k);
            int count                  = 4 - original_tiles[k] + 1;
            int effective_count        = 5 - original_tiles[k];

            detail.wait_counts[wait_tile_name] = effective_count;
            detail.total_wait_count += effective_count;
            detail.waiting_tiles.push_back(wait_tile_name);

            int tenpai_wait_count = 0;
            for (int w = 0; w < 34; ++w) {
              if (original_tiles[w] < 4) {
                original_tiles[w]++;
                int check_win =
                    ShantenAlgo::CalculateStandard(original_tiles, melds_count);
                if (check_win == -1) {
                  tenpai_wait_count += (5 - original_tiles[w]);
                }
                original_tiles[w]--;
              }
            }

            if (tenpai_wait_count >= 6) {
              detail.good_shape_count += effective_count;
            }
          }
          original_tiles[k]--;
        }
      }

      if (detail.total_wait_count > 0) {
        detail.good_shape_rate =
            (double)detail.good_shape_count / detail.total_wait_count * 100.0;
        result.analysis.push_back(detail);
      }

      original_tiles[i]++;
    }
  }

  std::sort(result.analysis.begin(),
            result.analysis.end(),
            [](const ShantenDetail& a, const ShantenDetail& b) {
              if (a.total_wait_count != b.total_wait_count)
                return a.total_wait_count > b.total_wait_count;
              return a.discard_tile < b.discard_tile;
            });
}

} // namespace calc
