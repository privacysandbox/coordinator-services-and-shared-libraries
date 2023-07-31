/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstring>
#include <memory>
#include <set>
#include <string>

#include "core/common/serialization/src/serialization.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/journal_service_interface.h"
#include "core/journal_service/src/error_codes.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

static constexpr uint64_t kCheckpointMetadataMagicNumber = 0x123456789;
static constexpr size_t kLogHeaderByteLength =
    3 * sizeof(uint64_t) + sizeof(uint16_t) + 4 * sizeof(uint64_t);
static constexpr google::scp::core::Version kCurrentVersion = {.major = 1,
                                                               .minor = 0};

namespace google::scp::core::journal_service {

class JournalSerialization {
 public:
  /**
   * @brief Used to serialize a log header section. Log headers are stored in
   * the following format:
   * [V-16B][TS-8B][JLS-2B][C-Hid-8B][C.Lid-8B][Log.Hid-8B][Log.Lid-8B]
   * @param bytes_buffer The bytes buffer to serialize the header to.
   * @param buffer_offset The offset to write the log header to.
   * @param timestamp The timestamp of the operation.
   * @param log_status The status of the log.
   * @param log_id The id of the component.
   * @param log_id The id of the log.
   * @param bytes_serialized Total bytes serialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult SerializeLogHeader(
      BytesBuffer& bytes_buffer, const size_t buffer_offset,
      const Timestamp& timestamp, const JournalLogStatus& log_status,
      const core::common::Uuid& component_id, const core::common::Uuid& log_id,
      size_t& bytes_serialized) {
    bytes_serialized = 0;

    // Serializing version
    size_t current_bytes_serialized = 0;
    auto execution_result = core::common::Serialization::Serialize(
        bytes_buffer, buffer_offset + bytes_serialized, kCurrentVersion,
        current_bytes_serialized);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_serialized += current_bytes_serialized;

    // Serializing timestamp.
    current_bytes_serialized = 0;
    execution_result = core::common::Serialization::Serialize(
        bytes_buffer, buffer_offset + bytes_serialized, timestamp,
        current_bytes_serialized);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_serialized += current_bytes_serialized;

    // Serializing log status.
    uint16_t log_status_value = static_cast<uint16_t>(log_status);
    current_bytes_serialized = 0;
    execution_result = core::common::Serialization::Serialize(
        bytes_buffer, buffer_offset + bytes_serialized, log_status_value,
        current_bytes_serialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_serialized += current_bytes_serialized;

    // Serializing component id.
    current_bytes_serialized = 0;
    execution_result = core::common::Serialization::Serialize(
        bytes_buffer, buffer_offset + bytes_serialized, component_id,
        current_bytes_serialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_serialized += current_bytes_serialized;

    // Serializing log id.
    current_bytes_serialized = 0;
    execution_result = core::common::Serialization::Serialize(
        bytes_buffer, buffer_offset + bytes_serialized, log_id,
        current_bytes_serialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_serialized += current_bytes_serialized;
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to serialize the last checkpoint metadata object into a buffer.
   *
   * @param bytes_buffer The bytes buffer to serialize the last checkpoint
   * object to.
   * @param buffer_offset The offset to write the last checkpoint object to.
   * @param last_checkpoint_metadata The last checkpoint metadata object to
   * serialize.
   * @param bytes_serialized Total bytes serialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult SerializeLastCheckpointMetadata(
      BytesBuffer& bytes_buffer, const size_t buffer_offset,
      const journal_service::LastCheckpointMetadata& last_checkpoint_metadata,
      size_t& bytes_serialized) {
    bytes_serialized = 0;
    return Serialize(bytes_buffer, buffer_offset, last_checkpoint_metadata,
                     bytes_serialized);
  }

  /**
   * @brief Used to serialize a checkpoint metadata object into a buffer.
   *
   * @param bytes_buffer The bytes buffer to serialize the checkpoint object to.
   * @param buffer_offset The offset to write the checkpoint object to.
   * @param checkpoint_metadata The checkpoint metadata object to
   * serialize.
   * @param bytes_serialized Total bytes serialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult SerializeCheckpointMetadata(
      BytesBuffer& bytes_buffer, const size_t buffer_offset,
      const journal_service::CheckpointMetadata& checkpoint_metadata,
      size_t& bytes_serialized) {
    // Each checkpoint file has a metadata object associated with which is
    // serialized at the end of the file. The format is:
    // [metadata][metadata header+body length][magic number]
    bytes_serialized = 0;
    size_t current_bytes_serialized = 0;
    auto execution_result =
        Serialize(bytes_buffer, buffer_offset, checkpoint_metadata,
                  current_bytes_serialized);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    bytes_serialized += current_bytes_serialized;
    current_bytes_serialized = 0;

    uint64_t size_of_metadata = bytes_serialized;
    execution_result = common::Serialization::Serialize(
        bytes_buffer, buffer_offset + bytes_serialized, size_of_metadata,
        current_bytes_serialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }

    bytes_serialized += current_bytes_serialized;
    current_bytes_serialized = 0;

    execution_result = common::Serialization::Serialize(
        bytes_buffer, buffer_offset + bytes_serialized,
        kCheckpointMetadataMagicNumber, current_bytes_serialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }

    bytes_serialized += current_bytes_serialized;
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to serialize a journal log object into a buffer.
   *
   * @param bytes_buffer The bytes buffer to serialize the journal log to.
   * @param buffer_offset The offset to write the journal log to.
   * @param journal_log The journal log object to serialize.
   * @param bytes_serialized Total bytes serialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult SerializeJournalLog(
      BytesBuffer& bytes_buffer, const size_t buffer_offset,
      const journal_service::JournalLog& journal_log,
      size_t& bytes_serialized) {
    bytes_serialized = 0;
    return Serialize(bytes_buffer, buffer_offset, journal_log,
                     bytes_serialized);
  }

  /**
   * @brief Used to serialize a protobuf object into a buffer.
   *
   * @param bytes_buffer The bytes buffer to serialize the protobuf object to.
   * @param buffer_offset The offset to write the protobuf object to.
   * @param object_to_serialize The protobuf object to serialize.
   * @param bytes_serialized Total bytes serialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  template <typename TProto>
  static ExecutionResult Serialize(BytesBuffer& bytes_buffer,
                                   const size_t buffer_offset,
                                   const TProto& object_to_serialize,
                                   size_t& bytes_serialized) {
    bytes_serialized = 0;
    return core::common::Serialization::Serialize<google::protobuf::Message>(
        bytes_buffer, buffer_offset, object_to_serialize, bytes_serialized);
  }

  /**
   * @brief Calculates the total serialization length of protobuf object.
   *
   * @param object_to_serialize The object to serialize.
   * @param byte_size Total bytes required for serialization of the object.
   * @return ExecutionResult The Execution results of the operation.
   */
  template <typename TProto>
  static ExecutionResult CalculateSerializationByteSize(
      const TProto& object_to_serialize, size_t& byte_size) {
    byte_size = 0;
    return core::common::Serialization::CalculateSerializationByteSize(
        object_to_serialize, byte_size);
  }

  /**
   * @brief Used fast forward the buffer offset and skip the log body part of
   * the data.
   *
   * @param bytes_buffer The bytes buffer to skip bytes.
   * @param buffer_offset The offset to skip bytes from.
   * @param bytes_skipped Total bytes skipped after the operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult SkipLogBody(const BytesBuffer& bytes_buffer,
                                     const size_t buffer_offset,
                                     size_t& bytes_skipped) {
    // Read the first uint64_t for the size of log body
    bytes_skipped = 0;
    uint64_t log_body_size = 0;
    size_t bytes_deserialized = 0;
    auto execution_result = common::Serialization::Deserialize(
        bytes_buffer, buffer_offset, log_body_size, bytes_deserialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }

    bytes_skipped = bytes_deserialized + log_body_size;
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to deserialize a log header section. Log headers are stored in
   * the following format:
   * [V-16B][TS-8B][JLS-2B][C-Hid-8B][C.Lid-8B][Log.Hid-8B][Log.Lid-8B]
   * @param bytes_buffer The bytes buffer to deserialize the header from.
   * @param buffer_offset The offset to write the log header from.
   * @param timestamp The timestamp of the operation.
   * @param log_status The status of the log.
   * @param component_id The id of the component.
   * @param log_id The id of the log.
   * @param bytes_deserialized Total bytes deserialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult DeserializeLogHeader(const BytesBuffer& bytes_buffer,
                                              const size_t buffer_offset,
                                              Timestamp& timestamp,
                                              JournalLogStatus& log_status,
                                              core::common::Uuid& component_id,
                                              core::common::Uuid& log_id,
                                              size_t& bytes_deserialized) {
    bytes_deserialized = 0;

    // Deserialize version.
    size_t current_bytes_deserialized = 0;
    Version version = {.major = UINT64_MAX, .minor = UINT64_MAX};
    auto execution_result = core::common::Serialization::Deserialize(
        bytes_buffer, buffer_offset + bytes_deserialized, version,
        current_bytes_deserialized);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_deserialized += current_bytes_deserialized;

    if (version.major != kCurrentVersion.major ||
        version.minor != kCurrentVersion.minor) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_VERSION_IS_INVALID);
    }

    // Deserialize timestamp.
    current_bytes_deserialized = 0;
    execution_result = core::common::Serialization::Deserialize(
        bytes_buffer, buffer_offset + bytes_deserialized, timestamp,
        current_bytes_deserialized);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_deserialized += current_bytes_deserialized;

    // Deserialize log status.
    uint16_t log_status_value = 0;
    current_bytes_deserialized = 0;
    execution_result = core::common::Serialization::Deserialize(
        bytes_buffer, buffer_offset + bytes_deserialized, log_status_value,
        current_bytes_deserialized);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_deserialized += current_bytes_deserialized;
    log_status = static_cast<JournalLogStatus>(log_status_value);

    // Deserialize component id.
    current_bytes_deserialized = 0;
    execution_result = core::common::Serialization::Deserialize(
        bytes_buffer, buffer_offset + bytes_deserialized, component_id,
        current_bytes_deserialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_deserialized += current_bytes_deserialized;

    // Deserialize log id.
    current_bytes_deserialized = 0;
    execution_result = core::common::Serialization::Deserialize(
        bytes_buffer, buffer_offset + bytes_deserialized, log_id,
        current_bytes_deserialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }
    bytes_deserialized += current_bytes_deserialized;
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to deserialize the last checkpoint metadata object into a
   * buffer.
   *
   * @param bytes_buffer The bytes buffer to deserialize the last checkpoint
   * metadata from.
   * @param buffer_offset The offset to write the the last checkpoint
   * metadata from.
   * @param last_checkpoint_metadata The last checkpoint metadata object to
   * deserialize.
   * @param bytes_deserialized Total bytes deserialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult DeserializeLastCheckpointMetadata(
      const BytesBuffer& bytes_buffer, const size_t buffer_offset,
      journal_service::LastCheckpointMetadata& last_checkpoint_metadata,
      size_t& bytes_deserialized) {
    bytes_deserialized = 0;
    return Deserialize(bytes_buffer, buffer_offset, last_checkpoint_metadata,
                       bytes_deserialized);
  }

  /**
   * @brief Used to deserialize a checkpoint metadata object into a buffer.
   *
   * @param bytes_buffer The bytes buffer to serialize the checkpoint metadata
   * object from.
   * @param buffer_offset The offset to write the checkpoint metadata object
   * from.
   * @param checkpoint_metadata The checkpoint metadata object to
   * serialize.
   * @param bytes_deserialized Total bytes deserialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult DeserializeCheckpointMetadata(
      const BytesBuffer& bytes_buffer, const size_t buffer_offset,
      journal_service::CheckpointMetadata& checkpoint_metadata,
      size_t& bytes_deserialized) {
    // Each checkpoint file has a metadata object associated with which is
    // serialized at the end of the file. The format is:
    // [metadata][metadata header+body length][magic number]
    bytes_deserialized = 0;
    uint64_t magic_number = 0;
    size_t current_bytes_deserialized = 0;
    size_t magic_number_buffer_offset = 0;

    if (bytes_buffer.length < sizeof(uint64_t)) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
    }

    magic_number_buffer_offset = bytes_buffer.length - sizeof(uint64_t);

    auto execution_result = core::common::Serialization::Deserialize(
        bytes_buffer, magic_number_buffer_offset, magic_number,
        current_bytes_deserialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }

    bytes_deserialized += current_bytes_deserialized;
    current_bytes_deserialized = 0;

    if (magic_number != kCheckpointMetadataMagicNumber) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_MAGIC_NUMBER_NOT_MATCHING);
    }

    // Going back in the stream to read the metadata length.
    uint64_t metadata_size = 0;
    size_t metadata_length_offset = 0;

    if (magic_number_buffer_offset < sizeof(uint64_t)) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
    }

    metadata_length_offset = magic_number_buffer_offset - sizeof(uint64_t);
    if (magic_number_buffer_offset >= bytes_buffer.length) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
    }

    execution_result = core::common::Serialization::Deserialize(
        bytes_buffer, metadata_length_offset, metadata_size,
        current_bytes_deserialized);

    if (!execution_result.Successful()) {
      return FailureExecutionResult(errors::SC_JOURNAL_SERVICE_CORRUPTED_BLOB);
    }
    bytes_deserialized += current_bytes_deserialized;
    current_bytes_deserialized = 0;

    size_t metadata_offset = 0;
    if (metadata_length_offset < metadata_size) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
    }

    metadata_offset = metadata_length_offset - metadata_size;
    if (metadata_offset >= bytes_buffer.length) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
    }

    execution_result =
        Deserialize(bytes_buffer, metadata_offset, checkpoint_metadata,
                    current_bytes_deserialized);

    if (!execution_result.Successful()) {
      return execution_result;
    }

    bytes_deserialized += current_bytes_deserialized;
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to deserialize a journal log object into a buffer.
   *
   * @param bytes_buffer The bytes buffer to deserialize the journal log from.
   * @param buffer_offset The offset to write the journal log from.
   * @param journal_log The journal log object to deserialize.
   * @param bytes_deserialized Total bytes deserialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  static ExecutionResult DeserializeJournalLog(
      const BytesBuffer& bytes_buffer, const size_t buffer_offset,
      journal_service::JournalLog& journal_log, size_t& bytes_deserialized) {
    bytes_deserialized = 0;
    return Deserialize(bytes_buffer, buffer_offset, journal_log,
                       bytes_deserialized);
  }

  /**
   * @brief Used to deserialize a protobuf object into a buffer.
   *
   * @param bytes_buffer The bytes buffer to deserialize the protobuf object
   * from.
   * @param buffer_offset The offset to write the protobuf object from.
   * @param object_to_deserialize The object to deserialize.
   * @param bytes_deserialized Total bytes deserialized after this operation.
   * @return ExecutionResult The Execution results of the operation.
   */
  template <typename TProto>
  static ExecutionResult Deserialize(const BytesBuffer& bytes_buffer,
                                     const size_t buffer_offset,
                                     TProto& object_to_deserialize,
                                     size_t& bytes_deserialized) {
    bytes_deserialized = 0;
    return core::common::Serialization::Deserialize<google::protobuf::Message>(
        bytes_buffer, buffer_offset, object_to_deserialize, bytes_deserialized);
  }
};

/**
 * @brief Calculates the total serialization length of checkpoint metadata
 * object.
 *
 * @param object_to_serialize The object to serialize.
 * @param byte_size Total bytes required for serialization of the object.
 * @return ExecutionResult The Execution results of the operation.
 */
template <>
inline ExecutionResult JournalSerialization::CalculateSerializationByteSize<
    journal_service::CheckpointMetadata>(
    const journal_service::CheckpointMetadata& object_to_serialize,
    size_t& byte_size) {
  byte_size = 0;
  auto execution_result =
      core::common::Serialization::CalculateSerializationByteSize(
          object_to_serialize, byte_size);

  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Adding magic number + size of metadata
  byte_size += sizeof(uint64_t) + sizeof(uint64_t);
  return SuccessExecutionResult();
}
}  // namespace google::scp::core::journal_service
