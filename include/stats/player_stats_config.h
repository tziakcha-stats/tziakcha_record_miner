#pragma once

#include <cstddef>
#include <string>

namespace tziakcha {
namespace stats {

struct PlayerStatsConfig {
  static constexpr const char* kScriptPlayers  = "p";
  static constexpr const char* kScriptWinFlags = "b";
  static constexpr const char* kScriptWins     = "y";
  static constexpr const char* kWinFanTotal    = "f";
  static constexpr const char* kWinFanMap      = "t";
  static constexpr const char* kWinHand        = "h";

  static constexpr const char* kStep          = "step";
  static constexpr const char* kStepPlayers   = "p";
  static constexpr const char* kStepTimestamp = "t";
  static constexpr const char* kStepActions   = "a";

  static constexpr const char* kEloField        = "e";
  static constexpr const char* kRecordId        = "id";
  static constexpr const char* kSessionId       = "belongs";
  static constexpr const char* kRecordTimestamp = "t";

  static constexpr std::size_t kActionTimeIndex = 2;
};

} // namespace stats
} // namespace tziakcha
