#pragma once

#include "analyzer/simulator.h"
#include <string>
#include <vector>

namespace tziakcha {
namespace analyzer {

class RecordAnalyzer {
public:
  RecordAnalyzer();

  SimulationResult Analyze(const std::string& record_json_str);

  static RecordAnalyzer& GetInstance();

private:
  RecordSimulator simulator_;
};

} // namespace analyzer
} // namespace tziakcha
