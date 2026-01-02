#include "analyzer/core.h"
#include <glog/logging.h>

namespace tziakcha {
namespace analyzer {

RecordAnalyzer::RecordAnalyzer() : simulator_() {}

SimulationResult RecordAnalyzer::Analyze(const std::string& record_json_str) {
  try {
    return simulator_.Simulate(record_json_str);
  } catch (const std::exception& e) {
    SimulationResult result;
    result.success       = false;
    result.error_message = std::string("Analysis error: ") + e.what();
    LOG(ERROR) << result.error_message;
    return result;
  }
}

RecordAnalyzer& RecordAnalyzer::GetInstance() {
  static RecordAnalyzer instance;
  return instance;
}

} // namespace analyzer
} // namespace tziakcha
