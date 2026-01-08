#ifndef SHANTEN_RESULT_H
#define SHANTEN_RESULT_H

#include <string>
#include <vector>
#include <map>

namespace calc {

struct ShantenDetail {
  std::string discard_tile;
  std::vector<std::string> waiting_tiles;

  std::map<std::string, int> wait_counts;
  int total_wait_count   = 0;
  int good_shape_count   = 0;
  double good_shape_rate = 0.0;
};

struct ShantenResult {
  int standard         = 99;
  int seven_pairs      = 99;
  int thirteen_orphans = 99;
  int all_unrelated    = 99;
  int knitted_dragon   = 99;

  std::vector<ShantenDetail> knitted_dragon_details;

  std::vector<ShantenDetail> analysis;
  std::vector<std::string> waiting_tiles;
};

} // namespace calc

#endif // SHANTEN_RESULT_H
