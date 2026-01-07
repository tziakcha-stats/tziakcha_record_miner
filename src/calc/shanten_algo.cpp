#include "calc/shanten_algo.h"
#include "base/mahjong_constants.h"
#include <algorithm>
#include <vector>
#include <cmath>

namespace calc {

namespace {

void RunStandardRecursive(
    std::vector<int>& tiles,
    int index,
    int current_sets,
    int current_taals,
    int& max_score) {
  if (current_sets + current_taals > 4)
    return;

  int score = current_sets * 10 + current_taals;
  if (score > max_score) {
    max_score = score;
  }

  if (index >= 34)
    return;

  if (tiles[index] == 0) {
    RunStandardRecursive(
        tiles, index + 1, current_sets, current_taals, max_score);
    return;
  }

  if (tiles[index] >= 3) {
    tiles[index] -= 3;
    RunStandardRecursive(
        tiles, index, current_sets + 1, current_taals, max_score);
    tiles[index] += 3;
  }

  if (index < 27 && index % 9 < 7) {
    if (tiles[index] >= 1 && tiles[index + 1] >= 1 && tiles[index + 2] >= 1) {
      tiles[index]--;
      tiles[index + 1]--;
      tiles[index + 2]--;
      RunStandardRecursive(
          tiles, index, current_sets + 1, current_taals, max_score);
      tiles[index]++;
      tiles[index + 1]++;
      tiles[index + 2]++;
    }
  }

  if (tiles[index] >= 2) {
    tiles[index] -= 2;
    RunStandardRecursive(
        tiles, index, current_sets, current_taals + 1, max_score);
    tiles[index] += 2;
  }

  if (index < 27 && index % 9 < 8) {
    if (tiles[index] >= 1 && tiles[index + 1] >= 1) {
      tiles[index]--;
      tiles[index + 1]--;
      RunStandardRecursive(
          tiles, index, current_sets, current_taals + 1, max_score);
      tiles[index]++;
      tiles[index + 1]++;
    }
  }

  if (index < 27 && index % 9 < 7) {
    if (tiles[index] >= 1 && tiles[index + 2] >= 1) {
      tiles[index]--;
      tiles[index + 2]--;
      RunStandardRecursive(
          tiles, index, current_sets, current_taals + 1, max_score);
      tiles[index]++;
      tiles[index + 2]++;
    }
  }

  RunStandardRecursive(
      tiles, index + 1, current_sets, current_taals, max_score);
}

} // namespace

int ShantenAlgo::CalculateStandard(const std::vector<int>& tiles_in,
                                   int melds_count) {
  int min_shanten        = 8;
  std::vector<int> tiles = tiles_in;

  for (int i = 0; i < 34; ++i) {
    if (tiles[i] >= 2) {
      tiles[i] -= 2;
      int max_score = 0;
      RunStandardRecursive(tiles, 0, 0, 0, max_score);
      int sets    = max_score / 10;
      int taatsus = max_score % 10;

      int total_sets = sets + melds_count;
      if (total_sets + taatsus > 4) {
        taatsus = 4 - total_sets;
      }

      int shanten = 8 - 2 * total_sets - taatsus - 1;
      if (shanten < min_shanten)
        min_shanten = shanten;

      tiles[i] += 2;
    }
  }

  {
    int max_score = 0;
    RunStandardRecursive(tiles, 0, 0, 0, max_score);
    int sets    = max_score / 10;
    int taatsus = max_score % 10;

    int total_sets = sets + melds_count;
    if (total_sets + taatsus > 4) {
      taatsus = 4 - total_sets;
    }

    int shanten = 8 - 2 * total_sets - taatsus;
    if (shanten < min_shanten)
      min_shanten = shanten;
  }

  return min_shanten;
}

int ShantenAlgo::CalculateSevenPairs(const std::vector<int>& tiles) {
  int pairs = 0;
  for (int i = 0; i < 34; ++i) {
    if (tiles[i] >= 2) {
      pairs++;
    }
  }
  return 6 - pairs;
}

int ShantenAlgo::CalculateThirteenOrphans(const std::vector<int>& tiles) {
  int types_count = 0;
  bool has_pair   = false;

  for (int idx : tziakcha::base::SHANTEN_ORPHANS) {
    if (tiles[idx] > 0) {
      types_count++;
      if (tiles[idx] >= 2)
        has_pair = true;
    }
  }

  return 13 - types_count - (has_pair ? 1 : 0);
}

int ShantenAlgo::CalculateAllUnrelated(const std::vector<int>& tiles) {
  int max_valid_count = 0;

  for (int p = 0; p < 6; ++p) {
    int current_count = 0;

    for (int i = 27; i < 34; ++i) {
      if (tiles[i] > 0)
        current_count++;
    }

    for (int s = 0; s < 3; ++s) {
      int pattern_idx = tziakcha::base::SHANTEN_PERMUTATIONS[p][s];
      int suit_base   = s * 9;
      for (int k = 0; k < 3; ++k) {
        int tile_idx =
            suit_base + tziakcha::base::SHANTEN_OFFSETS[pattern_idx][k];
        if (tiles[tile_idx] > 0)
          current_count++;
      }
    }

    if (current_count > max_valid_count)
      max_valid_count = current_count;
  }

  return 13 - max_valid_count;
}

int ShantenAlgo::CalculateKnittedDragon(const std::vector<int>& tiles_in,
                                        int melds_count) {
  int min_shanten = 99;

  for (int p = 0; p < 6; ++p) {
    std::vector<int> current_tiles = tiles_in;
    int knitted_found              = 0;

    for (int s = 0; s < 3; ++s) {
      int pattern_idx = tziakcha::base::SHANTEN_PERMUTATIONS[p][s];
      int suit_base   = s * 9;
      for (int k = 0; k < 3; ++k) {
        int tile_idx =
            suit_base + tziakcha::base::SHANTEN_OFFSETS[pattern_idx][k];
        if (current_tiles[tile_idx] > 0) {
          current_tiles[tile_idx]--;
          knitted_found++;
        }
      }
    }

    int missing_knitted = 9 - knitted_found;
    int max_score       = 0;

    int sub_min_shanten    = 99;
    bool pair_found_in_rem = false;

    for (int i = 0; i < 34; ++i) {
      if (current_tiles[i] >= 2) {
        current_tiles[i] -= 2;
        pair_found_in_rem = true;

        int local_score = 0;
        RunStandardRecursive(current_tiles, 0, 0, 0, local_score);
        int s = local_score / 10;
        int t = local_score % 10;

        int useful_sets    = std::min(s, 1);
        int useful_taatsus = 0;
        if (useful_sets < 1)
          useful_taatsus = std::min(t, 1);

        int val = 2 - 2 * useful_sets - useful_taatsus - 1;
        if (val < sub_min_shanten)
          sub_min_shanten = val;

        current_tiles[i] += 2;
      }
    }

    {
      int local_score = 0;
      RunStandardRecursive(current_tiles, 0, 0, 0, local_score);
      int s = local_score / 10;
      int t = local_score % 10;

      int useful_sets    = std::min(s, 1);
      int useful_taatsus = 0;
      if (useful_sets == 0)
        useful_taatsus = std::min(t, 1);

      int val = 2 - 2 * useful_sets - useful_taatsus;
      if (val < sub_min_shanten)
        sub_min_shanten = val;
    }

    int total_shanten = missing_knitted + sub_min_shanten;
    if (total_shanten < min_shanten)
      min_shanten = total_shanten;
  }

  return min_shanten;
}

} // namespace calc
