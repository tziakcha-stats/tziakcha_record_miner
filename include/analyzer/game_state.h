#pragma once

#include <array>
#include <vector>

namespace tziakcha {
namespace analyzer {

class GameState {
public:
  GameState();

  void Reset();

  void SetupWallAndDeal(const std::vector<int>& wall_indices,
                        const std::array<int, 4>& dice,
                        int dealer_idx);

  std::vector<int>& GetPlayerHand(int player_idx);
  const std::vector<int>& GetPlayerHand(int player_idx) const;

  std::vector<std::vector<int>>& GetPlayerPacks(int player_idx);
  const std::vector<std::vector<int>>& GetPlayerPacks(int player_idx) const;

  std::vector<int>& GetPlayerPackDirections(int player_idx);
  const std::vector<int>& GetPlayerPackDirections(int player_idx) const;

  std::vector<int>& GetPlayerPackOfferSequences(int player_idx);
  const std::vector<int>& GetPlayerPackOfferSequences(int player_idx) const;

  std::vector<int>& GetPlayerDiscards(int player_idx);
  const std::vector<int>& GetPlayerDiscards(int player_idx) const;

  int GetFlowerCount(int player_idx) const;
  void AddFlowerCount(int player_idx);

  std::vector<int>& GetPlayerFlowerTiles(int player_idx);
  const std::vector<int>& GetPlayerFlowerTiles(int player_idx) const;

  int GetCurrentPlayerIdx() const;
  void SetCurrentPlayerIdx(int idx);

  int GetDealerIdx() const;
  void SetDealerIdx(int idx);

  int GetWallFrontPtr() const;
  int GetWallBackPtr() const;
  void AdvanceWallFrontPtr(int count);
  void AdvanceWallBackPtr(int count);

  const std::vector<int>& GetWall() const;

  int GetLastDrawTile(int player_idx) const;
  void SetLastDrawTile(int player_idx, int tile);

  bool IsLastActionKong() const;
  void SetLastActionKong(bool value);

  bool IsLastActionAddKong() const;
  void SetLastActionAddKong(bool value);

  int GetLastDiscardTile() const;
  int GetLastDiscardPlayer() const;
  void SetLastDiscard(int player_idx, int tile);

  std::vector<int>& GetInitialHand(int player_idx);
  const std::vector<int>& GetInitialHand(int player_idx) const;

private:
  std::array<std::vector<int>, 4> hands_;
  std::array<std::vector<std::vector<int>>, 4> packs_;
  std::array<std::vector<int>, 4> pack_directions_;
  std::array<std::vector<int>, 4> pack_offer_sequences_;
  std::array<std::vector<int>, 4> discards_;
  std::array<int, 4> flower_counts_;
  std::array<std::vector<int>, 4> flower_tiles_;
  std::array<std::vector<int>, 4> initial_hands_;

  std::vector<int> wall_;
  int wall_front_ptr_;
  int wall_back_ptr_;

  int current_player_idx_;
  int dealer_idx_;

  std::array<int, 4> last_draw_tiles_;
  bool last_action_was_kong_;
  bool last_action_was_add_kong_;

  int last_discard_tile_;
  int last_discard_player_;

  void DealInitialTiles(int dealer_idx);
};

} // namespace analyzer
} // namespace tziakcha
