#include "utils/gb_format_converter.h"
#include "utils/tile.h"
#include <algorithm>
#include <array>
#include <map>
#include <sstream>

namespace tziakcha {
namespace utils {

std::string GBFormatConverter::TileIndexToGB(int tile_idx) {
  return GetTileChar(tile_idx) + std::string(1, GetSuitChar(tile_idx));
}

char GBFormatConverter::GetSuitChar(int tile_idx) {
  if (tile_idx < 0 || tile_idx >= 148) {
    return '?';
  }

  if (tile_idx < 36) {
    return 'm';
  } else if (tile_idx < 72) {
    return 's';
  } else if (tile_idx < 108) {
    return 'p';
  } else if (tile_idx < 136) {
    return 'z';
  } else {
    return 'h';
  }
}

std::string GBFormatConverter::GetTileChar(int tile_idx) {
  if (tile_idx < 0 || tile_idx >= 148) {
    return "?";
  }

  if (tile_idx < 108) {
    int base = tile_idx >> 2;
    int num  = (base % 9) + 1;
    return std::to_string(num);
  } else if (tile_idx < 136) {
    int base                                = (tile_idx - 108) >> 2;
    const std::array<std::string, 7> honors = {
        "E", "S", "W", "N", "C", "F", "P"};
    return honors[base];
  } else {
    int flower_idx                           = tile_idx - 136;
    const std::array<std::string, 8> flowers = {
        "a", "b", "c", "d", "e", "f", "g", "h"};
    if (flower_idx < 8) {
      return flowers[flower_idx];
    }
    return "?";
  }
}

std::string GBFormatConverter::ConvertHandTilesToGB(
    const std::vector<int>& hand_tiles, bool sort_tiles) {
  if (hand_tiles.empty()) {
    return "";
  }

  std::vector<int> tiles = hand_tiles;
  if (sort_tiles) {
    std::sort(tiles.begin(), tiles.end());
  }

  std::map<char, std::vector<int>> suits;
  for (int tile : tiles) {
    char suit = GetSuitChar(tile);
    int num   = 0;

    if (tile < 108) {
      int base = tile >> 2;
      num      = (base % 9) + 1;
      suits[suit].push_back(num);
    } else if (tile < 136) {
      suits['z'].push_back(tile);
    }
  }

  std::string result;

  for (char suit_char : {'m', 'p', 's'}) {
    if (suits[suit_char].empty()) {
      continue;
    }

    std::sort(suits[suit_char].begin(), suits[suit_char].end());
    for (int num : suits[suit_char]) {
      result += std::to_string(num);
    }
    result += suit_char;
  }

  if (!suits['z'].empty()) {
    std::sort(suits['z'].begin(), suits['z'].end());
    for (int tile : suits['z']) {
      result += GetTileChar(tile);
    }
  }

  return result;
}

std::string GBFormatConverter::ConvertPackToGB(
    const std::vector<int>& pack_tiles, int offer_direction) {
  if (pack_tiles.empty()) {
    return "";
  }

  std::ostringstream oss;
  oss << "[";

  for (int tile : pack_tiles) {
    if (tile < 108) {
      int base = tile >> 2;
      int num  = (base % 9) + 1;
      oss << num;
    } else if (tile < 136) {
      oss << GetTileChar(tile);
    }
  }

  if (!pack_tiles.empty()) {
    char suit = GetSuitChar(pack_tiles[0]);
    if (suit == 'z') {
    } else {
      oss << suit;
    }
  }

  if (offer_direction > 0 && offer_direction != 4) {
    oss << "," << offer_direction;
  }

  oss << "]";
  return oss.str();
}

std::string GBFormatConverter::BuildCompleteHandString(
    const std::vector<int>& hand_tiles,
    const std::vector<std::vector<int>>& packs,
    const std::vector<int>& pack_directions,
    int win_tile,
    bool is_self_drawn) {
  std::ostringstream result;

  for (size_t i = 0; i < packs.size(); ++i) {
    if (!packs[i].empty()) {
      int direction = (i < pack_directions.size()) ? pack_directions[i] : 0;
      result << ConvertPackToGB(packs[i], direction);
    }
  }

  if (is_self_drawn && win_tile >= 0) {
    std::vector<int> hand_without_win = hand_tiles;
    auto it =
        std::find(hand_without_win.begin(), hand_without_win.end(), win_tile);
    if (it != hand_without_win.end()) {
      hand_without_win.erase(it);
    }

    result << ConvertHandTilesToGB(hand_without_win, true);

    if (win_tile < 108) {
      int base = win_tile >> 2;
      int num  = (base % 9) + 1;
      result << num << GetSuitChar(win_tile);
    } else if (win_tile < 136) {
      result << GetTileChar(win_tile);
    }
  } else {
    result << ConvertHandTilesToGB(hand_tiles, true);

    if (!is_self_drawn && win_tile >= 0) {
      if (win_tile < 108) {
        int base = win_tile >> 2;
        int num  = (base % 9) + 1;
        result << num << GetSuitChar(win_tile);
      } else if (win_tile < 136) {
        result << GetTileChar(win_tile);
      }
    }
  }

  return result.str();
}

std::string GBFormatConverter::BuildEnvFlag(
    char round_wind,
    char seat_wind,
    bool is_self_drawn,
    bool is_last_copy,
    bool is_sea_last,
    bool is_robbing_kong) {
  std::ostringstream oss;
  oss << round_wind << seat_wind;
  oss << (is_self_drawn ? '1' : '0');
  oss << (is_last_copy ? '1' : '0');
  oss << (is_sea_last ? '1' : '0');
  oss << (is_robbing_kong ? '1' : '0');
  return oss.str();
}

std::string GBFormatConverter::BuildFlowerString(
    int flower_count, const std::vector<int>& flower_tiles) {
  if (flower_count == 0) {
    return "";
  }

  if (!flower_tiles.empty()) {
    std::ostringstream oss;
    for (int tile : flower_tiles) {
      if (tile >= 136 && tile < 148) {
        oss << GetTileChar(tile);
      }
    }
    return oss.str();
  }

  return std::to_string(flower_count);
}

std::string GBFormatConverter::BuildFullGBString(
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
    const std::vector<int>& flower_tiles) {
  std::ostringstream result;

  result << BuildCompleteHandString(
      hand_tiles, packs, pack_directions, win_tile, is_self_drawn);

  result << "|"
         << BuildEnvFlag(round_wind,
                         seat_wind,
                         is_self_drawn,
                         is_last_copy,
                         is_sea_last,
                         is_robbing_kong);

  std::string flower_str = BuildFlowerString(flower_count, flower_tiles);
  if (!flower_str.empty()) {
    result << "|" << flower_str;
  }

  return result.str();
}

} // namespace utils
} // namespace tziakcha
