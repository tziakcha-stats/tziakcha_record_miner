#ifndef TZIAKCHA_RECORD_PARSER_H
#define TZIAKCHA_RECORD_PARSER_H

#include <nlohmann/json.hpp>
#include <string>
#include <memory>

using json = nlohmann::json;

namespace tziakcha {
namespace fetcher {

class RecordParser {
public:
  static json parse_script(const std::string& encoded_script);

  static json merge_record_with_script(const json& record_json);
};

} // namespace fetcher
} // namespace tziakcha

#endif
