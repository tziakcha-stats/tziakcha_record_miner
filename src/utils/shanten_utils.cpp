#include "utils/shanten_utils.h"

namespace tziakcha {
namespace utils {

std::vector<int> ConvertHandToAlgo(const mahjong::Handtiles& hand) {
  std::vector<int> tiles(34, 0);

  for (const auto& kv : hand.lipai_table) {
    int gb_id = kv.first;
    int count = kv.second;
    if (count == 0 || gb_id == 0)
      continue;

    int algo_id = -1;
    if (gb_id >= 1 && gb_id <= 9) {
      algo_id = gb_id - 1;
    } else if (gb_id >= 10 && gb_id <= 18) {
      algo_id = gb_id + 8;
    } else if (gb_id >= 19 && gb_id <= 27) {
      algo_id = gb_id - 10;
    } else if (gb_id >= 28 && gb_id <= 31) {
      algo_id = gb_id - 1;
    } else if (gb_id == 32) {
      algo_id = 33;
    } else if (gb_id == 33) {
      algo_id = 32;
    } else if (gb_id == 34) {
      algo_id = 31;
    }

    if (algo_id >= 0 && algo_id < 34) {
      tiles[algo_id] = count;
    }
  }

  return tiles;
}

std::string GetAlgoTileName(int id) {
  if (id < 9)
    return std::to_string(id + 1) + "m";
  if (id < 18)
    return std::to_string(id - 9 + 1) + "p";
  if (id < 27)
    return std::to_string(id - 18 + 1) + "s";

  if (id == 27)
    return "E";
  if (id == 28)
    return "S";
  if (id == 29)
    return "W";
  if (id == 30)
    return "N";

  if (id == 31)
    return "P";
  if (id == 32)
    return "F";
  if (id == 33)
    return "C";
  return "?";
}

} // namespace utils
} // namespace tziakcha
