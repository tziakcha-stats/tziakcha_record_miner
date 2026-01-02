#include "utils/tile.h"
#include "base/mahjong_constants.h"

namespace tziakcha {
namespace utils {

bool Tile::IsValid(int index) { return index >= 0 && index < 144; }

std::string Tile::ToString(int index) {
  if (index >= 0 && index < 136) {
    return base::TILE_IDENTITY[index >> 2];
  }
  if (index >= 136 && index < 144) {
    return base::FLOWER_TILES[index - 136];
  }
  return "??";
}

std::string Tile::ToGBString(int index) {
  if (index >= 0 && index < 136) {
    return base::TILE_IDENTITY[index >> 2];
  }
  return "??";
}

} // namespace utils
} // namespace tziakcha
