#include "calc/fan_calculator.h"
#include <execinfo.h>
#include <glog/logging.h>
#include <sstream>
#include "print.h"
#include "console.h"

namespace calc {

namespace {
void LogStackTrace(const char* context) {
  void* buffer[64];
  int nptrs      = backtrace(buffer, 64);
  char** strings = backtrace_symbols(buffer, nptrs);
  std::ostringstream oss;
  oss << "Stack trace (" << context << "):\n";
  for (int i = 0; i < nptrs; ++i) {
    oss << "  " << strings[i] << "\n";
  }
  free(strings);
  LOG(ERROR) << oss.str();
}
} // namespace

FanCalculator::FanCalculator() : is_parsed_(false), is_calculated_(false) {
  LOG(INFO) << "FanCalculator initialized";
}

FanCalculator::~FanCalculator() { LOG(INFO) << "FanCalculator destroyed"; }

bool FanCalculator::ParseHandtiles(const std::string& handtiles_str) {
  LOG(INFO) << "Parsing handtiles string: " << handtiles_str;

  try {
    handtiles_.StringToHandtiles(handtiles_str);
    is_parsed_     = true;
    is_calculated_ = false;

    LOG(INFO) << "Handtiles parsed successfully";
    LOG(INFO) << "Standard format: " << handtiles_.HandtilesToString();

    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to parse handtiles: " << e.what();
    LogStackTrace("ParseHandtiles std::exception");
    is_parsed_ = false;
    return false;
  } catch (...) {
    LOG(ERROR) << "Failed to parse handtiles: unknown exception";
    LogStackTrace("ParseHandtiles unknown");
    is_parsed_ = false;
    return false;
  }
}

bool FanCalculator::IsWinningHand() const {
  if (!is_parsed_) {
    LOG(WARNING) << "Handtiles not parsed yet";
    return false;
  }

  mahjong::Fan temp_fan;
  bool is_winning = temp_fan.JudgeHu(handtiles_);

  LOG(INFO) << "IsWinningHand check result: "
            << (is_winning ? "true" : "false");

  return is_winning;
}

bool FanCalculator::CalculateFan() {
  if (!is_parsed_) {
    LOG(ERROR) << "Cannot calculate fan: handtiles not parsed";
    return false;
  }

  if (!IsWinningHand()) {
    LOG(WARNING) << "Cannot calculate fan: not a winning hand";
    return false;
  }

  LOG(INFO) << "Starting fan calculation";

  try {
    if (handtiles_.HandtilesToString() == "") {
      LOG(ERROR) << "handtiles.handtiles is null";
      return false;
    }

    fan_.CountFan(handtiles_);
    is_calculated_ = true;

    LOG(INFO) << "Fan calculation completed. Total fan: " << fan_.tot_fan_res;
    LOG(INFO) << "Number of different fan patterns: " << GetFanDetails().size();

    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception in CountFan: " << e.what();
    LogStackTrace("CalculateFan std::exception");
    return false;
  } catch (...) {
    LOG(ERROR) << "Unknown exception in CountFan";
    LogStackTrace("CalculateFan unknown");
    return false;
  }
}

int FanCalculator::GetTotalFan() const {
  if (!is_calculated_) {
    LOG(WARNING) << "Fan not calculated yet";
    return 0;
  }

  return fan_.tot_fan_res;
}

std::string FanCalculator::GetStandardHandtilesString() const {
  if (!is_parsed_) {
    LOG(WARNING) << "Handtiles not parsed yet";
    return "";
  }

  return handtiles_.HandtilesToString();
}

std::vector<FanResult> FanCalculator::GetFanDetails() const {
  std::vector<FanResult> results;

  if (!is_calculated_) {
    LOG(WARNING) << "Fan not calculated yet";
    return results;
  }

  LOG(INFO) << "Collecting fan details";

  for (int i = 1; i < mahjong::FAN_SIZE; i++) {
    if (fan_.fan_table_res[i].empty()) {
      continue;
    }

    for (size_t j = 0; j < fan_.fan_table_res[i].size(); j++) {
      FanResult result;
      result.fan_name  = mahjong::FAN_NAME[i];
      result.fan_score = mahjong::FAN_SCORE[i];

      for (auto pid : fan_.fan_table_res[i][j]) {
        std::string pack_desc = PackToEmojiString(fan_.fan_packs_res[pid]);
        result.pack_descriptions.push_back(pack_desc);
      }

      results.push_back(result);

      LOG(INFO) << "Fan detail: " << result.fan_name << " (" << result.fan_score
                << " fan) " << "with " << result.pack_descriptions.size()
                << " pack(s)";
    }
  }

  LOG(INFO) << "Total fan details collected: " << results.size();

  return results;
}

std::vector<FanTypeInfo> FanCalculator::GetFanTypesSummary() const {
  std::vector<FanTypeInfo> summary;

  if (!is_calculated_) {
    LOG(WARNING) << "Fan not calculated yet";
    return summary;
  }

  for (int i = 1; i < mahjong::FAN_SIZE; i++) {
    int count = fan_.fan_table_res[i].size();
    if (count > 0) {
      FanTypeInfo info;
      info.fan_type      = static_cast<mahjong::fan_t>(i);
      info.fan_name      = mahjong::FAN_NAME[i];
      info.count         = count;
      info.score_per_fan = mahjong::FAN_SCORE[i];
      info.total_score   = count * mahjong::FAN_SCORE[i];
      summary.push_back(info);

      LOG(INFO) << "Fan type: " << info.fan_name << ", count: " << info.count
                << ", score_per_fan: " << info.score_per_fan
                << ", total: " << info.total_score;
    }
  }

  return summary;
}

int FanCalculator::GetFanTypeCount(mahjong::fan_t fan_type) const {
  if (!is_calculated_) {
    LOG(WARNING) << "Fan not calculated yet";
    return 0;
  }

  if (fan_type <= 0 || fan_type >= mahjong::FAN_SIZE) {
    LOG(ERROR) << "Invalid fan type: " << fan_type;
    return 0;
  }

  return fan_.fan_table_res[fan_type].size();
}

bool FanCalculator::HasFanType(mahjong::fan_t fan_type) const {
  return GetFanTypeCount(fan_type) > 0;
}

std::vector<mahjong::fan_t> FanCalculator::GetAllFanTypes() const {
  std::vector<mahjong::fan_t> types;

  if (!is_calculated_) {
    LOG(WARNING) << "Fan not calculated yet";
    return types;
  }

  for (int i = 1; i < mahjong::FAN_SIZE; i++) {
    if (fan_.fan_table_res[i].size() > 0) {
      types.push_back(static_cast<mahjong::fan_t>(i));
    }
  }

  return types;
}

} // namespace calc
