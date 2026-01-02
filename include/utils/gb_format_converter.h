#pragma once

#include <string>
#include <vector>

namespace tziakcha {
namespace utils {

class GBFormatConverter {
public:
  static std::string ConvertHandTilesToGB(const std::vector<int>& hand_tiles,
                                          bool sort_tiles = true);

  static std::string
  ConvertPackToGB(const std::vector<int>& pack_tiles, int offer_direction = 0);

  static std::string BuildCompleteHandString(
      const std::vector<int>& hand_tiles,
      const std::vector<std::vector<int>>& packs,
      const std::vector<int>& pack_directions,
      int win_tile = -1);

  static std::string BuildEnvFlag(
      char round_wind,
      char seat_wind,
      bool is_self_drawn,
      bool is_last_copy,
      bool is_sea_last,
      bool is_robbing_kong);

  static std::string BuildFlowerString(
      int flower_count, const std::vector<int>& flower_tiles = {});

  static std::string BuildFullGBString(
      const std::vector<int>& hand_tiles,
      const std::vector<std::vector<int>>& packs,
      const std::vector<int>& pack_directions,
      int win_tile,
      char round_wind,
      char seat_wind,
      bool is_self_drawn,
      bool is_last_copy,
      bool is_sea_last,
      bool is_robbing_kong,
      int flower_count,
      const std::vector<int>& flower_tiles = {});

private:
  static std::string TileIndexToGB(int tile_idx);
  static char GetSuitChar(int tile_idx);
  static std::string GetTileChar(int tile_idx);
};

} // namespace utils
} // namespace tziakcha
