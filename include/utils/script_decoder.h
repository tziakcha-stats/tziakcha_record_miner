#pragma once

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace tziakcha {
namespace utils {

bool DecodeScriptToJson(const std::string& encoded, json& out);

} // namespace utils
} // namespace tziakcha
