#pragma once

#include "analyzer/game_state.h"
#include "analyzer/record_parser.h"

namespace tziakcha {
namespace analyzer {

struct PackInfo {
  enum class Type { Chi, Peng, Gang } type;
  int base_tile;
  int offer_direction;
};

class ActionProcessor {
public:
  ActionProcessor(GameState& state);

  void ProcessAction(const Action& action);

  bool ProcessDraw(int player_idx, int tile, int time_ms);
  bool
  ProcessDiscard(int player_idx, int tile, bool is_hand_played, int time_ms);
  bool ProcessFlowerReplacement(
      int player_idx, int flower_tile, int replacement_tile, bool is_auto);
  bool ProcessChiAction(int player_idx, int data);
  bool ProcessPengAction(int player_idx, int base_tile, int offer_direction);
  bool ProcessGangAction(int player_idx, int base_tile, int data);
  bool ProcessWin(int player_idx, const json& win_data);
  bool ProcessPass(int player_idx);
  bool ProcessAbandonment(int player_idx);

private:
  GameState& state_;

  void RemoveTileFromHand(int player_idx, int tile);
  void RemoveNTilesFromHand(int player_idx, int tile_base, int count);
  int FindTileInHand(int player_idx, int tile_base);
  bool HasTileInHand(int player_idx, int tile) const;
};

} // namespace analyzer
} // namespace tziakcha
