#include <gtest/gtest.h>
#include "fetcher/record_parser.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace tziakcha::fetcher;

TEST(RecordParserTest, MergeRecordWithoutScript) {
  json record;
  record["record_id"] = "test_456";
  record["title"]     = "Game Without Script";

  json merged = RecordParser::merge_record_with_script(record);

  EXPECT_FALSE(merged.contains("step"));
  EXPECT_EQ(merged["record_id"], "test_456");
}

TEST(RecordParserTest, ParseScriptEmptyString) {
  json parsed = RecordParser::parse_script("");

  EXPECT_TRUE(parsed.is_object());
  EXPECT_TRUE(parsed.empty());
}

TEST(RecordParserTest, ParseScriptInvalidBase64) {
  std::string invalid_script = "!@#$%^&*()";

  json parsed = RecordParser::parse_script(invalid_script);

  EXPECT_TRUE(parsed.is_object());
  EXPECT_TRUE(parsed.empty());
}

TEST(RecordParserTest, MergeRecordWithInvalidScript) {
  json record;
  record["record_id"] = "test_789";
  record["script"]    = "invalid_base64_data";

  json merged = RecordParser::merge_record_with_script(record);

  EXPECT_EQ(merged["record_id"], "test_789");
}
