#include "analyzer/game_state.h"
#include "base/mahjong_constants.h"
#include <algorithm>
#include <glog/logging.h>

namespace tziakcha {
namespace analyzer {

GameState::GameState()
    : wall_front_ptr_(0),
      wall_back_ptr_(0),
      current_player_idx_(-1),
      dealer_idx_(0),
      last_action_was_kong_(false),
      last_discard_tile_(-1),
      last_discard_player_(-1) {
  flower_counts_.fill(0);
  last_draw_tiles_.fill(-1);
}

void GameState::Reset() {
  for (int i = 0; i < 4; ++i) {
    hands_[i].clear();
    packs_[i].clear();
    pack_directions_[i].clear();
    discards_[i].clear();
    flower_counts_[i] = 0;
    flower_tiles_[i].clear();
    initial_hands_[i].clear();
    last_draw_tiles_[i] = -1;
  }

  wall_.clear();
  wall_front_ptr_       = 0;
  wall_back_ptr_        = 0;
  current_player_idx_   = -1;
  dealer_idx_           = 0;
  last_action_was_kong_ = false;
  last_discard_tile_    = -1;
  last_discard_player_  = -1;
}

void GameState::SetupWallAndDeal(const std::vector<int>& wall_indices,
                                 const std::array<int, 4>& dice,
                                 int dealer_idx) {
  dealer_idx_ = dealer_idx;

  int wall_break_pos = (dealer_idx - (dice[0] + dice[1] - 1) + 12) % 4;
  int start_pos =
      (wall_break_pos * 36) + (dice[0] + dice[1] + dice[2] + dice[3]) * 2;
  start_pos = start_pos % 144;

  LOG(INFO) << "Setting up wall with break position at index: "
            << wall_break_pos << ", start_pos: " << start_pos;

  wall_.clear();
  wall_.insert(
      wall_.end(), wall_indices.begin() + start_pos, wall_indices.end());
  wall_.insert(
      wall_.end(), wall_indices.begin(), wall_indices.begin() + start_pos);

  wall_front_ptr_ = 0;
  wall_back_ptr_  = wall_.size() - 1;

  LOG(INFO) << "DEBUG: After wall setup, wall_ size=" << wall_.size();
  LOG(INFO) << "DEBUG: First 20 tiles in wall_:";
  for (size_t i = 0; i < std::min(size_t(20), wall_.size()); ++i) {
    LOG(INFO) << "  wall_[" << i << "] = " << wall_[i];
  }

  DealInitialTiles(dealer_idx);

  current_player_idx_ = dealer_idx;
}

void GameState::DealInitialTiles(int dealer_idx) {
  for (int i = 0; i < 3; ++i) {
    for (int p_offset = 0; p_offset < 4; ++p_offset) {
      int player_idx = (dealer_idx + p_offset) % 4;
      for (int j = 0; j < 4; ++j) {
        hands_[player_idx].push_back(wall_[wall_front_ptr_++]);
      }
    }
  }

  for (int p_offset = 0; p_offset < 4; ++p_offset) {
    int player_idx = (dealer_idx + p_offset) % 4;
    hands_[player_idx].push_back(wall_[wall_front_ptr_++]);
  }

  hands_[dealer_idx].push_back(wall_[wall_front_ptr_++]);

  for (int i = 0; i < 4; ++i) {
    std::sort(hands_[i].begin(), hands_[i].end());
    initial_hands_[i] = hands_[i];

    LOG(INFO) << "DEBUG: Player " << i << " hand after deal:";
    for (size_t j = 0; j < hands_[i].size(); ++j) {
      LOG(INFO) << "  [" << j << "] tile value=" << hands_[i][j]
                << ", base=" << (hands_[i][j] >> 2)
                << ", identity=" << base::TILE_IDENTITY[hands_[i][j] >> 2];
    }
  }
}

std::vector<int>& GameState::GetPlayerHand(int player_idx) {
  return hands_[player_idx];
}

const std::vector<int>& GameState::GetPlayerHand(int player_idx) const {
  return hands_[player_idx];
}

std::vector<std::vector<int>>& GameState::GetPlayerPacks(int player_idx) {
  return packs_[player_idx];
}

const std::vector<std::vector<int>>&
GameState::GetPlayerPacks(int player_idx) const {
  return packs_[player_idx];
}

std::vector<int>& GameState::GetPlayerPackDirections(int player_idx) {
  return pack_directions_[player_idx];
}

const std::vector<int>&
GameState::GetPlayerPackDirections(int player_idx) const {
  return pack_directions_[player_idx];
}

std::vector<int>& GameState::GetPlayerDiscards(int player_idx) {
  return discards_[player_idx];
}

const std::vector<int>& GameState::GetPlayerDiscards(int player_idx) const {
  return discards_[player_idx];
}

int GameState::GetFlowerCount(int player_idx) const {
  return flower_counts_[player_idx];
}

void GameState::AddFlowerCount(int player_idx) { flower_counts_[player_idx]++; }

std::vector<int>& GameState::GetPlayerFlowerTiles(int player_idx) {
  return flower_tiles_[player_idx];
}

const std::vector<int>& GameState::GetPlayerFlowerTiles(int player_idx) const {
  return flower_tiles_[player_idx];
}

int GameState::GetCurrentPlayerIdx() const { return current_player_idx_; }

void GameState::SetCurrentPlayerIdx(int idx) { current_player_idx_ = idx; }

int GameState::GetDealerIdx() const { return dealer_idx_; }

void GameState::SetDealerIdx(int idx) { dealer_idx_ = idx; }

int GameState::GetWallFrontPtr() const { return wall_front_ptr_; }

int GameState::GetWallBackPtr() const { return wall_back_ptr_; }

void GameState::AdvanceWallFrontPtr(int count) { wall_front_ptr_ += count; }

const std::vector<int>& GameState::GetWall() const { return wall_; }

int GameState::GetLastDrawTile(int player_idx) const {
  return last_draw_tiles_[player_idx];
}

void GameState::SetLastDrawTile(int player_idx, int tile) {
  last_draw_tiles_[player_idx] = tile;
}

bool GameState::IsLastActionKong() const { return last_action_was_kong_; }

void GameState::SetLastActionKong(bool value) { last_action_was_kong_ = value; }

int GameState::GetLastDiscardTile() const { return last_discard_tile_; }

int GameState::GetLastDiscardPlayer() const { return last_discard_player_; }

void GameState::SetLastDiscard(int player_idx, int tile) {
  last_discard_player_ = player_idx;
  last_discard_tile_   = tile;
}

std::vector<int>& GameState::GetInitialHand(int player_idx) {
  return initial_hands_[player_idx];
}

const std::vector<int>& GameState::GetInitialHand(int player_idx) const {
  return initial_hands_[player_idx];
}

} // namespace analyzer
} // namespace tziakcha
