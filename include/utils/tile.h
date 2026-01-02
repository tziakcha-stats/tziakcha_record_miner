#pragma once

#include <string>

namespace tziakcha {
namespace utils {

class Tile {
public:
  static bool IsValid(int index);
  static std::string ToString(int index);
  static std::string ToGBString(int index);
};

} // namespace utils
} // namespace tziakcha
