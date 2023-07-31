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

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/journal_service_interface.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::common::Uuid;
using google::scp::core::journal_service::CheckpointMetadata;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::LastCheckpointMetadata;
using std::make_shared;
using std::vector;

namespace google::scp::core::test {
TEST(JournalServiceSerializationTests, LogHeaderSerialization) {
  BytesBuffer bytes_buffer(kLogHeaderByteLength);
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;

  JournalLogStatus log_status = JournalLogStatus::Log;
  Uuid component_uuid;
  component_uuid.high = 0x09999999;
  component_uuid.low = 0x08888888;

  Uuid log_uuid;
  log_uuid.high = 0x01111111;
  log_uuid.low = 0x02222222;

  Timestamp current_timestamp = 1234567890;

  EXPECT_EQ(JournalSerialization::SerializeLogHeader(
                bytes_buffer, buffer_offset, current_timestamp, log_status,
                component_uuid, log_uuid, bytes_serialized),
            SuccessExecutionResult());
  bytes_buffer.length += bytes_serialized;

  for (auto i = 0; i < 58; ++i) {
    BytesBuffer partial_buffer;
    partial_buffer.bytes = make_shared<vector<Byte>>(i);
    partial_buffer.capacity = i;
    partial_buffer.length = i;
    size_t partial_byte_serialized = 0;
    EXPECT_EQ(
        JournalSerialization::SerializeLogHeader(
            partial_buffer, buffer_offset, current_timestamp, log_status,
            component_uuid, log_uuid, partial_byte_serialized),
        FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));
    // [V-16B][TS-8B][JLS-2B][C-Hid-8B][C.Lid-8B][Log.Hid-8B][Log.Lid-8B]
    if (i < 58 && i >= 42) {
      EXPECT_EQ(partial_byte_serialized, 42);
    } else if (i < 42 && i >= 26) {
      EXPECT_EQ(partial_byte_serialized, 26);
    } else if (i < 26 && i >= 24) {
      EXPECT_EQ(partial_byte_serialized, 24);
    } else if (i < 24 && i >= 16) {
      EXPECT_EQ(partial_byte_serialized, 16);
    } else if (i < 16) {
      EXPECT_EQ(partial_byte_serialized, 0);
    }
  }

  JournalLogStatus deserialized_log_status;
  Timestamp deserialized_timestamp;
  Uuid deserialized_component_uuid;
  Uuid deserialized_log_uuid;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(JournalSerialization::DeserializeLogHeader(
                bytes_buffer, buffer_offset, deserialized_timestamp,
                deserialized_log_status, deserialized_component_uuid,
                deserialized_log_uuid, bytes_deserialized),
            SuccessExecutionResult());

  EXPECT_EQ(deserialized_log_status, log_status);
  EXPECT_EQ(deserialized_component_uuid, component_uuid);
  EXPECT_EQ(deserialized_timestamp, current_timestamp);
  EXPECT_EQ(deserialized_log_uuid, log_uuid);
  EXPECT_EQ(bytes_deserialized, bytes_serialized);

  for (auto i = 0; i < 58; ++i) {
    BytesBuffer partial_buffer;
    partial_buffer.bytes = make_shared<vector<Byte>>(i);
    partial_buffer.capacity = i;
    partial_buffer.length = i;
    size_t partial_byte_deserialized = 0;
    if (i < 16) {
      EXPECT_EQ(
          JournalSerialization::DeserializeLogHeader(
              partial_buffer, buffer_offset, current_timestamp, log_status,
              component_uuid, log_uuid, partial_byte_deserialized),
          FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
    } else {
      EXPECT_EQ(
          JournalSerialization::DeserializeLogHeader(
              partial_buffer, buffer_offset, current_timestamp, log_status,
              component_uuid, log_uuid, partial_byte_deserialized),
          FailureExecutionResult(errors::SC_SERIALIZATION_VERSION_IS_INVALID));
    }
    // [V-16B][TS-8B][JLS-2B][C-Hid-8B][C.Lid-8B][Log.Hid-8B][Log.Lid-8B]
    if (i >= 16) {
      EXPECT_EQ(partial_byte_deserialized, 16);
    } else if (i < 16) {
      EXPECT_EQ(partial_byte_deserialized, 0);
    }
  }
}

TEST(JournalServiceSerializationTests, LastCheckpointMetdataSerialization) {
  LastCheckpointMetadata last_checkpoint_metdata;
  last_checkpoint_metdata.set_last_checkpoint_id(1234567);
  size_t byte_size_required = 0;
  EXPECT_EQ(JournalSerialization::CalculateSerializationByteSize(
                last_checkpoint_metdata, byte_size_required),
            SuccessExecutionResult());

  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(byte_size_required);
  bytes_buffer.capacity = byte_size_required;
  bytes_buffer.length = byte_size_required;
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;

  EXPECT_EQ(JournalSerialization::SerializeLastCheckpointMetadata(
                bytes_buffer, buffer_offset, last_checkpoint_metdata,
                bytes_serialized),
            SuccessExecutionResult());

  for (size_t i = 0; i < byte_size_required; ++i) {
    BytesBuffer partial_buffer;
    partial_buffer.bytes = make_shared<vector<Byte>>(i);
    partial_buffer.capacity = i;
    partial_buffer.length = i;
    size_t partial_byte_serialized = 0;
    EXPECT_EQ(
        JournalSerialization::SerializeLastCheckpointMetadata(
            partial_buffer, buffer_offset, last_checkpoint_metdata,
            partial_byte_serialized),
        FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));

    EXPECT_EQ(partial_byte_serialized, 0);
  }

  LastCheckpointMetadata deserialized_last_checkpoint_metdata;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(JournalSerialization::DeserializeLastCheckpointMetadata(
                bytes_buffer, buffer_offset,
                deserialized_last_checkpoint_metdata, bytes_deserialized),
            SuccessExecutionResult());

  EXPECT_EQ(deserialized_last_checkpoint_metdata.last_checkpoint_id(),
            last_checkpoint_metdata.last_checkpoint_id());
  EXPECT_EQ(bytes_deserialized, bytes_serialized);

  for (size_t i = 0; i < byte_size_required; ++i) {
    BytesBuffer partial_buffer;
    partial_buffer.bytes = make_shared<vector<Byte>>(i);
    partial_buffer.capacity = i;
    partial_buffer.length = i;
    size_t partial_byte_deserialized = 0;
    if (i < 8) {
      EXPECT_EQ(
          JournalSerialization::DeserializeLastCheckpointMetadata(
              partial_buffer, buffer_offset,
              deserialized_last_checkpoint_metdata, partial_byte_deserialized),
          FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
    } else {
      EXPECT_EQ(
          JournalSerialization::DeserializeLastCheckpointMetadata(
              partial_buffer, buffer_offset,
              deserialized_last_checkpoint_metdata, partial_byte_deserialized),
          SuccessExecutionResult());
    }

    if (i >= 8) {
      EXPECT_EQ(partial_byte_deserialized, 8);
    } else {
      EXPECT_EQ(partial_byte_deserialized, 0);
    }
  }
}

TEST(JournalServiceSerializationTests, CheckpointMetdataSerialization) {
  CheckpointMetadata checkpoint_metdata;
  checkpoint_metdata.set_last_processed_journal_id(1234567);
  size_t byte_size_required = 0;
  EXPECT_EQ(JournalSerialization::CalculateSerializationByteSize(
                checkpoint_metdata, byte_size_required),
            SuccessExecutionResult());

  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(byte_size_required);
  bytes_buffer.capacity = byte_size_required;
  bytes_buffer.length = byte_size_required;
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;

  EXPECT_EQ(
      JournalSerialization::SerializeCheckpointMetadata(
          bytes_buffer, buffer_offset, checkpoint_metdata, bytes_serialized),
      SuccessExecutionResult());

  for (size_t i = 0; i < byte_size_required; ++i) {
    BytesBuffer partial_buffer;
    partial_buffer.bytes = make_shared<vector<Byte>>(i);
    partial_buffer.capacity = i;
    partial_buffer.length = i;
    size_t partial_byte_serialized = 0;
    EXPECT_EQ(
        JournalSerialization::SerializeCheckpointMetadata(
            partial_buffer, buffer_offset, checkpoint_metdata,
            partial_byte_serialized),
        FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));

    if (i >= 94) {
      EXPECT_EQ(partial_byte_serialized, 94);
    }
  }

  CheckpointMetadata deserialized_checkpoint_metdata;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(JournalSerialization::DeserializeCheckpointMetadata(
                bytes_buffer, buffer_offset, deserialized_checkpoint_metdata,
                bytes_deserialized),
            SuccessExecutionResult());

  EXPECT_EQ(checkpoint_metdata.last_processed_journal_id(),
            deserialized_checkpoint_metdata.last_processed_journal_id());
  EXPECT_EQ(bytes_deserialized, bytes_serialized);

  for (size_t i = 0; i < byte_size_required; ++i) {
    BytesBuffer partial_buffer;
    partial_buffer.bytes = make_shared<vector<Byte>>(i);
    partial_buffer.capacity = i;
    partial_buffer.length = i;
    size_t partial_byte_deserialized = 0;
    if (i < 8) {
      EXPECT_EQ(
          JournalSerialization::DeserializeCheckpointMetadata(
              partial_buffer, buffer_offset, deserialized_checkpoint_metdata,
              partial_byte_deserialized),
          FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
    } else {
      EXPECT_EQ(JournalSerialization::DeserializeCheckpointMetadata(
                    partial_buffer, buffer_offset,
                    deserialized_checkpoint_metdata, partial_byte_deserialized),
                FailureExecutionResult(
                    errors::SC_JOURNAL_SERVICE_MAGIC_NUMBER_NOT_MATCHING));
    }

    if (i >= 8) {
      EXPECT_EQ(partial_byte_deserialized, 8);
    } else {
      EXPECT_EQ(partial_byte_deserialized, 0);
    }
  }
}

TEST(JournalServiceSerializationTests, JournalLogSerialization) {
  JournalLog journal_log;
  journal_log.set_type(1234567);
  size_t byte_size_required = 0;
  EXPECT_EQ(JournalSerialization::CalculateSerializationByteSize(
                journal_log, byte_size_required),
            SuccessExecutionResult());

  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(byte_size_required);
  bytes_buffer.capacity = byte_size_required;
  bytes_buffer.length = byte_size_required;
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;

  EXPECT_EQ(JournalSerialization::SerializeJournalLog(
                bytes_buffer, buffer_offset, journal_log, bytes_serialized),
            SuccessExecutionResult());

  JournalLog deserialized_journal_log;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(JournalSerialization::DeserializeJournalLog(
                bytes_buffer, buffer_offset, deserialized_journal_log,
                bytes_deserialized),
            SuccessExecutionResult());

  EXPECT_EQ(journal_log.type(), deserialized_journal_log.type());
  EXPECT_EQ(bytes_deserialized, bytes_serialized);
}

}  // namespace google::scp::core::test
