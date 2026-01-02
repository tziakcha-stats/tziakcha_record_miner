#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "storage/filesystem_storage.h"

using json   = nlohmann::json;
namespace fs = std::filesystem;

class FileSystemStorageTest : public ::testing::Test {
protected:
  void SetUp() override {
    test_dir_ = fs::temp_directory_path() / "tziakcha_test";
    if (fs::exists(test_dir_)) {
      fs::remove_all(test_dir_);
    }
    fs::create_directories(test_dir_);

    storage_ = std::make_unique<tziakcha::storage::FileSystemStorage>(
        test_dir_.string());
  }

  void TearDown() override {
    if (fs::exists(test_dir_)) {
      fs::remove_all(test_dir_);
    }
  }

  fs::path test_dir_;
  std::unique_ptr<tziakcha::storage::FileSystemStorage> storage_;
};

TEST_F(FileSystemStorageTest, SaveAndLoadSimpleJson) {
  json test_data;
  test_data["name"] = "test_game";
  test_data["id"]   = 12345;
  test_data["tags"] = json::array({"tag1", "tag2"});

  EXPECT_TRUE(storage_->save_json("game/test", test_data));
  EXPECT_TRUE(storage_->exists("game/test"));

  json loaded_data;
  EXPECT_TRUE(storage_->load_json("game/test", loaded_data));
  EXPECT_EQ(loaded_data["name"], "test_game");
  EXPECT_EQ(loaded_data["id"], 12345);
  EXPECT_EQ(loaded_data["tags"].size(), 2);
}

TEST_F(FileSystemStorageTest, SaveCreateNestedDirectories) {
  json test_data;
  test_data["value"] = "nested";

  EXPECT_TRUE(storage_->save_json("deep/nested/path/data", test_data));
  EXPECT_TRUE(storage_->exists("deep/nested/path/data"));

  fs::path expected_path = test_dir_ / "deep" / "nested" / "path" / "data.json";
  EXPECT_TRUE(fs::exists(expected_path));
}

TEST_F(FileSystemStorageTest, ExistsReturnsFalseForMissingFile) {
  EXPECT_FALSE(storage_->exists("nonexistent/file"));
}

TEST_F(FileSystemStorageTest, LoadJsonFailsForMissingFile) {
  json data;
  EXPECT_FALSE(storage_->load_json("nonexistent/file", data));
}

TEST_F(FileSystemStorageTest, RemoveFile) {
  json test_data;
  test_data["value"] = "to_remove";

  EXPECT_TRUE(storage_->save_json("file/to/remove", test_data));
  EXPECT_TRUE(storage_->exists("file/to/remove"));

  EXPECT_TRUE(storage_->remove("file/to/remove"));
  EXPECT_FALSE(storage_->exists("file/to/remove"));
}

TEST_F(FileSystemStorageTest, RemoveNonexistentFileFails) {
  EXPECT_FALSE(storage_->remove("nonexistent/file"));
}

TEST_F(FileSystemStorageTest, ListKeysWithPrefix) {
  json test_data;
  test_data["value"] = 1;

  EXPECT_TRUE(storage_->save_json("records/game_001", test_data));
  test_data["value"] = 2;
  EXPECT_TRUE(storage_->save_json("records/game_002", test_data));
  test_data["value"] = 3;
  EXPECT_TRUE(storage_->save_json("records/game_003", test_data));
  test_data["value"] = 4;
  EXPECT_TRUE(storage_->save_json("other/game_001", test_data));

  auto keys = storage_->list_keys("records");
  EXPECT_EQ(keys.size(), 3);

  for (const auto& key : keys) {
    EXPECT_TRUE(key.find("records") == 0);
  }
}

TEST_F(FileSystemStorageTest, SaveOverwriteExistingFile) {
  json old_data;
  old_data["value"] = "old";
  EXPECT_TRUE(storage_->save_json("file/overwrite", old_data));

  json new_data;
  new_data["value"]   = "new";
  new_data["updated"] = true;
  EXPECT_TRUE(storage_->save_json("file/overwrite", new_data));

  json loaded_data;
  EXPECT_TRUE(storage_->load_json("file/overwrite", loaded_data));
  EXPECT_EQ(loaded_data["value"], "new");
  EXPECT_TRUE(loaded_data["updated"]);
}

TEST_F(FileSystemStorageTest, SaveComplexJson) {
  json complex_data;
  complex_data["game_id"] = 12345;
  complex_data["players"] = json::array();

  json player1;
  player1["name"]  = "Player1";
  player1["score"] = 8000;
  complex_data["players"].push_back(player1);

  json player2;
  player2["name"]  = "Player2";
  player2["score"] = 7500;
  complex_data["players"].push_back(player2);

  complex_data["metadata"]["date"]    = "2026-01-03";
  complex_data["metadata"]["version"] = "1.0";

  EXPECT_TRUE(storage_->save_json("games/complex", complex_data));

  json loaded;
  EXPECT_TRUE(storage_->load_json("games/complex", loaded));
  EXPECT_EQ(loaded["game_id"], 12345);
  EXPECT_EQ(loaded["players"].size(), 2);
  EXPECT_EQ(loaded["players"][0]["name"], "Player1");
  EXPECT_EQ(loaded["metadata"]["date"], "2026-01-03");
}

TEST_F(FileSystemStorageTest, SaveMultipleFilesIndependently) {
  for (int i = 1; i <= 5; ++i) {
    json data;
    data["id"]   = i;
    data["name"] = "record_" + std::to_string(i);
    EXPECT_TRUE(storage_->save_json("records/item_" + std::to_string(i), data));
  }

  for (int i = 1; i <= 5; ++i) {
    json loaded;
    EXPECT_TRUE(
        storage_->load_json("records/item_" + std::to_string(i), loaded));
    EXPECT_EQ(loaded["id"], i);
    EXPECT_EQ(loaded["name"], "record_" + std::to_string(i));
  }
}

TEST_F(FileSystemStorageTest, KeyWithSpecialCharactersInPath) {
  json test_data;
  test_data["value"] = "special";

  std::string key = "path/with-dash/and_underscore";
  EXPECT_TRUE(storage_->save_json(key, test_data));
  EXPECT_TRUE(storage_->exists(key));

  json loaded;
  EXPECT_TRUE(storage_->load_json(key, loaded));
  EXPECT_EQ(loaded["value"], "special");
}

TEST_F(FileSystemStorageTest, FileHasValidJsonFormat) {
  json test_data;
  test_data["key"]    = "value";
  test_data["number"] = 42;

  EXPECT_TRUE(storage_->save_json("validation/test", test_data));

  fs::path file_path = test_dir_ / "validation" / "test.json";
  std::ifstream file(file_path);
  ASSERT_TRUE(file.is_open());

  json file_content;
  EXPECT_NO_THROW(file >> file_content);
  file.close();

  EXPECT_EQ(file_content["key"], "value");
  EXPECT_EQ(file_content["number"], 42);
}
