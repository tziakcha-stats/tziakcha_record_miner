#include "analyzer/action.h"
#include "base/mahjong_constants.h"
#include "utils/tile.h"
#include <algorithm>
#include <glog/logging.h>

namespace tziakcha {
namespace analyzer {

ActionProcessor::ActionProcessor(GameState& state) : state_(state) {}

void ActionProcessor::ProcessAction(const Action& action) {
  int p_idx  = action.player_idx;
  int a_type = action.action_type;
  int data   = action.data;

  int lo_byte = data & 0xFF;
  int hi_byte = (data >> 8) & 0xFF;

  switch (a_type) {
  case 0:
    break;
  case 1: {
    int ot = (hi_byte & 15) + 136;
    state_.AddFlowerCount(p_idx);
    state_.GetPlayerFlowerTiles(p_idx).push_back(ot);
    RemoveTileFromHand(p_idx, ot);
    state_.GetPlayerHand(p_idx).push_back(lo_byte);
    state_.SetLastDrawTile(p_idx, lo_byte);
    break;
  }
  case 2: {
    state_.SetCurrentPlayerIdx(p_idx);
    int tile            = lo_byte;
    bool is_hand_played = ((hi_byte & 1) != 0);
    int play_mode       = (data >> 9) & 3;

    RemoveTileFromHand(p_idx, tile);
    state_.GetPlayerDiscards(p_idx).push_back(tile);
    state_.SetLastDiscard(p_idx, tile);
    state_.SetLastActionKong(false);

    LOG(INFO) << "  Player " << p_idx
              << " discarded: " << utils::Tile::ToString(tile)
              << (is_hand_played ? " (hand)" : " (drawn)");
    break;
  }
  case 3:
    ProcessChiAction(p_idx, data);
    break;
  case 4: {
    if (data == 0)
      break;
    int tile_val        = (data & 0x3F) << 2;
    int offer_direction = (data >> 6) & 3;
    ProcessPengAction(p_idx, tile_val, offer_direction);
    break;
  }
  case 5: {
    if (data == 0)
      break;
    int tile_val = (data & 0x3F) << 2;
    ProcessGangAction(p_idx, tile_val, data);
    break;
  }
  case 6: {
    bool is_auto  = (data & 1) != 0;
    int fan_count = data >> 1;

    LOG(INFO) << "  IMPORTANT: Player " << p_idx << " HU: "
              << (is_auto ? "auto" : "manual") << ", fans=" << fan_count;

    ProcessWin(p_idx, json{{"fans", fan_count}, {"is_auto", is_auto}});
    break;
  }
  case 7: {
    state_.SetCurrentPlayerIdx(p_idx);
    int tile_to_draw      = lo_byte;
    bool is_backward_draw = (hi_byte != 0);

    state_.GetPlayerHand(p_idx).push_back(tile_to_draw);
    state_.SetLastDrawTile(p_idx, tile_to_draw);

    LOG(INFO) << "  Player " << p_idx
              << " drew: " << utils::Tile::ToString(tile_to_draw)
              << (is_backward_draw ? " (from back)" : " (from front)");
    break;
  }
  case 8: {
    int pass_mode = data & 3;
    std::string mode_str =
        (pass_mode == 0)   ? "manual"
        : (pass_mode == 1) ? "auto"
                           : "forced";

    LOG(INFO) << "  Player " << p_idx << " passed (" << mode_str << "): "
              << "cannot chi/peng/gang";
    ProcessPass(p_idx);
    break;
  }
  case 9: {
    LOG(INFO) << "  Player " << p_idx << " abandoned declared win (弃)";
    ProcessAbandonment(p_idx);
    break;
  }
  default:
    break;
  }

  std::sort(state_.GetPlayerHand(p_idx).begin(),
            state_.GetPlayerHand(p_idx).end());
}

bool ActionProcessor::ProcessDraw(int player_idx, int tile, int time_ms) {
  state_.SetCurrentPlayerIdx(player_idx);
  state_.GetPlayerHand(player_idx).push_back(tile);
  state_.SetLastDrawTile(player_idx, tile);
  std::sort(state_.GetPlayerHand(player_idx).begin(),
            state_.GetPlayerHand(player_idx).end());
  return true;
}

bool ActionProcessor::ProcessDiscard(
    int player_idx, int tile, bool is_hand_played, int time_ms) {
  state_.SetCurrentPlayerIdx(player_idx);
  RemoveTileFromHand(player_idx, tile);
  state_.GetPlayerDiscards(player_idx).push_back(tile);
  state_.SetLastDiscard(player_idx, tile);
  state_.SetLastActionKong(false);
  return true;
}

bool ActionProcessor::ProcessFlowerReplacement(
    int player_idx, int flower_tile, int replacement_tile, bool is_auto) {
  state_.AddFlowerCount(player_idx);
  state_.GetPlayerFlowerTiles(player_idx).push_back(flower_tile);
  RemoveTileFromHand(player_idx, flower_tile);
  state_.GetPlayerHand(player_idx).push_back(replacement_tile);
  state_.SetLastDrawTile(player_idx, replacement_tile);
  return true;
}

bool ActionProcessor::ProcessChiAction(int player_idx, int data) {
  int tile_val        = (data & 0x3F) << 2;
  int offer_direction = (data >> 6) & 3;

  state_.SetCurrentPlayerIdx(player_idx);

  if (data == 0) {
    return false;
  }

  int offer_from_idx = (player_idx + offer_direction) % 4;
  int offer_tile     = state_.GetLastDiscardTile();

  if (tile_val - 4 + ((data >> 10) & 3) < 0) {
    tile_val = offer_tile;
  }

  int c1 = tile_val - 4 + ((data >> 10) & 3);
  int c2 = tile_val + ((data >> 12) & 3);
  int c3 = tile_val + 4 + ((data >> 14) & 3);

  std::vector<int> chi_tiles = {c1, c2, c3};

  for (int t = 0; t < 3; ++t) {
    if ((chi_tiles[t] >> 2) != (offer_tile >> 2)) {
      RemoveNTilesFromHand(player_idx, chi_tiles[t] >> 2, 1);
    }
  }

  state_.GetPlayerPacks(player_idx).push_back(chi_tiles);
  state_.GetPlayerPackDirections(player_idx).push_back(offer_direction);

  LOG(INFO) << "  Added CHI pack for player " << player_idx << ": "
            << utils::Tile::ToString(c1) << " " << utils::Tile::ToString(c2)
            << " " << utils::Tile::ToString(c3)
            << " (offer_tile: " << utils::Tile::ToString(offer_tile) << ")";

  try {
    if (!state_.GetPlayerDiscards(offer_from_idx).empty()) {
      state_.GetPlayerDiscards(offer_from_idx).pop_back();
      LOG(INFO) << "  Removed offer tile from player " << offer_from_idx
                << " discards";
    }
  } catch (...) {
    LOG(ERROR) << "  Failed to remove offer tile from player "
               << offer_from_idx;
  }

  return true;
}

bool ActionProcessor::ProcessPengAction(
    int player_idx, int base_tile, int offer_direction) {
  state_.SetCurrentPlayerIdx(player_idx);

  int offer_from_idx = (player_idx + offer_direction) % 4;
  int offer_tile     = state_.GetLastDiscardTile();

  RemoveNTilesFromHand(player_idx, base_tile >> 2, 2);

  std::vector<int> peng_tiles = {base_tile, base_tile, base_tile};
  state_.GetPlayerPacks(player_idx).push_back(peng_tiles);
  state_.GetPlayerPackDirections(player_idx).push_back(offer_direction);

  LOG(INFO) << "  Added PENG pack for player " << player_idx << ": "
            << utils::Tile::ToString(base_tile) << " x3"
            << " (offer_tile: " << utils::Tile::ToString(offer_tile) << ")";

  try {
    if (!state_.GetPlayerDiscards(offer_from_idx).empty()) {
      state_.GetPlayerDiscards(offer_from_idx).pop_back();
      LOG(INFO) << "  Removed offer tile from player " << offer_from_idx
                << " discards";
    }
  } catch (...) {
    LOG(ERROR) << "  Failed to remove offer tile from player "
               << offer_from_idx;
  }

  return true;
}

bool ActionProcessor::ProcessGangAction(
    int player_idx, int base_tile, int data) {
  state_.SetLastActionKong(true);
  state_.SetCurrentPlayerIdx(player_idx);

  int offer_direction = (data >> 6) & 3;
  bool is_add_kong    = (data & 0x0300) == 0x0300;
  bool is_concealed   = offer_direction == 0;

  LOG(INFO) << "  Processing GANG: base_tile="
            << utils::Tile::ToString(base_tile) << ", is_add_kong="
            << is_add_kong << ", is_concealed=" << is_concealed;

  std::vector<int> gang_tiles = {base_tile, base_tile, base_tile, base_tile};

  if (is_add_kong) {
    RemoveNTilesFromHand(player_idx, base_tile >> 2, 1);
    state_.SetLastDiscard(player_idx, base_tile);

    auto& packs     = state_.GetPlayerPacks(player_idx);
    auto& pack_dirs = state_.GetPlayerPackDirections(player_idx);

    for (size_t i = 0; i < packs.size(); ++i) {
      if (packs[i].size() == 3 && (packs[i][0] >> 2) == (base_tile >> 2)) {
        packs[i].push_back(base_tile);

        if (i < pack_dirs.size()) {
          pack_dirs[i] = 5 + pack_dirs[i];
        }
        LOG(INFO) << "  Upgraded PENG to GANG for player " << player_idx << ": "
                  << utils::Tile::ToString(base_tile) << " x4 (add kong)";
        return true;
      }
    }

    LOG(WARNING) << "  Failed to find PENG to upgrade for add kong";
    return false;

  } else if (is_concealed) {
    RemoveNTilesFromHand(player_idx, base_tile >> 2, 4);
    state_.GetPlayerPacks(player_idx).push_back(gang_tiles);
    state_.GetPlayerPackDirections(player_idx).push_back(0);

    LOG(INFO) << "  Added concealed GANG for player " << player_idx << ": "
              << utils::Tile::ToString(base_tile) << " x4 (concealed)";

  } else {
    RemoveNTilesFromHand(player_idx, base_tile >> 2, 3);
    state_.GetPlayerPacks(player_idx).push_back(gang_tiles);
    state_.GetPlayerPackDirections(player_idx).push_back(offer_direction);

    int offer_from_idx = (player_idx + offer_direction) % 4;
    try {
      if (!state_.GetPlayerDiscards(offer_from_idx).empty()) {
        state_.GetPlayerDiscards(offer_from_idx).pop_back();
        LOG(INFO) << "  Removed offer tile from player " << offer_from_idx
                  << " discards";
      }
    } catch (...) {
      LOG(ERROR) << "  Failed to remove offer tile from player "
                 << offer_from_idx;
    }

    LOG(INFO) << "  Added melded GANG for player " << player_idx << ": "
              << utils::Tile::ToString(base_tile) << " x4 (melded)";
  }

  return true;
}

bool ActionProcessor::ProcessWin(int player_idx, const json& win_data) {
  state_.SetCurrentPlayerIdx(player_idx);
  state_.SetLastActionKong(false);

  int fan_count = 0;
  bool is_auto  = false;

  if (win_data.contains("fans")) {
    fan_count = win_data["fans"];
  }
  if (win_data.contains("is_auto")) {
    is_auto = win_data["is_auto"];
  }

  if (fan_count == 0) {
    LOG(WARNING) << "  Win attempt by player " << player_idx
                 << " is INVALID (0 fans - 错和)";
  } else {
    LOG(INFO) << "  Player " << player_idx << " won with " << fan_count
              << " fan(s) (" << (is_auto ? "auto" : "manual") << ")";
  }

  return true;
}

bool ActionProcessor::ProcessPass(int player_idx) {
  LOG(INFO) << "  Player " << player_idx << " cannot chi/peng/gang (pass)";
  return true;
}

bool ActionProcessor::ProcessAbandonment(int player_idx) {
  LOG(WARNING) << "  Player " << player_idx << " abandoned (win invalid - 弃)";
  return true;
}

void ActionProcessor::RemoveTileFromHand(int player_idx, int tile) {
  auto& hand = state_.GetPlayerHand(player_idx);
  auto it    = std::find(hand.begin(), hand.end(), tile);
  if (it != hand.end()) {
    hand.erase(it);
  }
}

void ActionProcessor::RemoveNTilesFromHand(
    int player_idx, int tile_base, int count) {
  auto& hand  = state_.GetPlayerHand(player_idx);
  int removed = 0;
  for (auto it = hand.begin(); it != hand.end() && removed < count;) {
    if ((*it >> 2) == tile_base) {
      it = hand.erase(it);
      removed++;
    } else {
      ++it;
    }
  }
}

int ActionProcessor::FindTileInHand(int player_idx, int tile_base) {
  const auto& hand = state_.GetPlayerHand(player_idx);
  for (int tile : hand) {
    if ((tile >> 2) == tile_base) {
      return tile;
    }
  }
  return -1;
}

bool ActionProcessor::HasTileInHand(int player_idx, int tile) const {
  const auto& hand = state_.GetPlayerHand(player_idx);
  return std::find(hand.begin(), hand.end(), tile) != hand.end();
}

} // namespace analyzer
} // namespace tziakcha
