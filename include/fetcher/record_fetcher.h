#ifndef TZIAKCHA_RECORD_FETCHER_H
#define TZIAKCHA_RECORD_FETCHER_H

#include "storage/storage.h"
#include <memory>
#include <string>

namespace tziakcha {
namespace fetcher {

class RecordFetcher {
public:
  explicit RecordFetcher(std::shared_ptr<storage::Storage> storage);

  bool fetch_record(const std::string& record_id,
                    const std::string& output_key = "");

private:
  std::shared_ptr<storage::Storage> storage_;
};

} // namespace fetcher
} // namespace tziakcha

#endif
