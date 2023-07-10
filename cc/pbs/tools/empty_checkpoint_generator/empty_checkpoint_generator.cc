// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <atomic>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/journal_service/src/error_codes.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/journal_utils.h"
#include "core/journal_service/src/proto/journal_service.pb.h"

using google::scp::core::JournalLogStatus;

using google::scp::core::Timestamp;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalUtils;
using std::atomic;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::move;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;

int main() {
  // Unique wall-clock timestamp is used for checkpoint_id
  Timestamp current_clock =
      TimeProvider::GetUniqueWallTimestampInNanoseconds().count();
  google::scp::core::CheckpointId checkpoint_id = current_clock;
  google::scp::core::JournalId last_processed_journal_id = current_clock;

  {
    google::scp::core::journal_service::LastCheckpointMetadata
        last_checkpoint_metadata;
    last_checkpoint_metadata.set_last_checkpoint_id(checkpoint_id);

    size_t byte_serialized = 0;
    auto buffer = make_shared<google::scp::core::BytesBuffer>(1000);
    auto result = JournalSerialization::SerializeLastCheckpointMetadata(
        *buffer, 0, last_checkpoint_metadata, byte_serialized);
    if (result != google::scp::core::SuccessExecutionResult()) {
      std::cerr << "Error serializing last_checkpoint" << std::endl;
      return -1;
    }

    std::cout << "last_checkpoint serialized bytes size " << byte_serialized
              << std::endl;

    std::ofstream outfile;
    outfile.open("last_checkpoint", std::ios::binary | std::ios::out);
    outfile.write(buffer->bytes->data(), byte_serialized);
    outfile.close();
  }

  {
    google::scp::core::journal_service::CheckpointMetadata checkpoint_metadata;
    checkpoint_metadata.set_last_processed_journal_id(
        last_processed_journal_id);

    size_t byte_serialized = 0;
    auto buffer = make_shared<google::scp::core::BytesBuffer>(1000);
    auto result = JournalSerialization::SerializeCheckpointMetadata(
        *buffer, 0, checkpoint_metadata, byte_serialized);
    if (result != google::scp::core::SuccessExecutionResult()) {
      std::cerr << "Error serializing checkpoint" << std::endl;
      return -1;
    }

    std::cout << "checkpoint serialized bytes size " << byte_serialized
              << std::endl;

    std::ofstream outfile;
    outfile.open("checkpoint_0" + std::to_string(current_clock),
                 std::ios::binary | std::ios::out);
    outfile.write(buffer->bytes->data(), byte_serialized);
    outfile.close();
  }
}
