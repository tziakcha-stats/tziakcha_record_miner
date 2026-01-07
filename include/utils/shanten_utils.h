#ifndef TZIAKCHA_UTILS_SHANTEN_UTILS_H
#define TZIAKCHA_UTILS_SHANTEN_UTILS_H

#include "handtiles.h"
#include <vector>
#include <string>

namespace tziakcha {
namespace utils {

std::vector<int> ConvertHandToAlgo(const mahjong::Handtiles& hand);

std::string GetAlgoTileName(int id);

} // namespace utils
} // namespace tziakcha

#endif // TZIAKCHA_UTILS_SHANTEN_UTILS_H
