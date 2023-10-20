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
#include <string>
#include <type_traits>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/type_def.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core::common {
/**
 * @brief Used for all De/Serialization for primitives. Supporting
 * numbers/Uuid/protobuf.
 */

class Serialization {
 public:
  /**
   * @brief Used to serialize a T value into bytes buffer.
   *
   * @tparam T The type of the object.
   * @param bytes_buffer The bytes buffer to serialize the object to.
   * @param buffer_offset The offset on the buffer to serialize the object to.
   * @param value_to_serialize The value of the object to serialize.
   * @param bytes_serialized The total bytes serialized after the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  template <typename T>
  static ExecutionResult Serialize(core::BytesBuffer& bytes_buffer,
                                   const size_t buffer_offset,
                                   const T& value_to_serialize,
                                   size_t& bytes_serialized) {
    if (!std::is_integral_v<T>) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_INVALID_SERIALIZATION_TYPE);
    }

    bytes_serialized = 0;
    size_t size_of_object_in_bytes = sizeof(T);
    if (buffer_offset + size_of_object_in_bytes > bytes_buffer.capacity) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE);
    }
    Byte* data_ptr = bytes_buffer.bytes->data() + buffer_offset;
    memcpy(data_ptr, &value_to_serialize, size_of_object_in_bytes);
    bytes_serialized = size_of_object_in_bytes;
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to deserialize a T value from bytes buffer.
   *
   * @tparam T The type of the object.
   * @param bytes_buffer The bytes buffer to deserialize the object from.
   * @param buffer_offset The offset on the buffer to deserialize the object
   * from.
   * @param value_to_deserialize The value of the object to deserialize.
   * @param bytes_serialized The total bytes deserialized after the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  template <typename T>
  static ExecutionResult Deserialize(const core::BytesBuffer& bytes_buffer,
                                     const size_t buffer_offset,
                                     T& value_to_deserialize,
                                     size_t& bytes_deserialized) {
    if (!std::is_integral_v<T>) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_INVALID_SERIALIZATION_TYPE);
    }

    bytes_deserialized = 0;
    size_t size_of_object_in_bytes = sizeof(T);
    if (buffer_offset + size_of_object_in_bytes > bytes_buffer.length) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
    }
    Byte* data_ptr = bytes_buffer.bytes->data() + buffer_offset;
    memcpy(&value_to_deserialize, data_ptr, size_of_object_in_bytes);
    bytes_deserialized = size_of_object_in_bytes;
    return SuccessExecutionResult();
  }

  static ExecutionResult CalculateSerializationByteSize(
      const google::protobuf::Message& value_to_serialize, size_t& byte_size) {
    size_t size_of_object_size_block = sizeof(uint64_t);
    uint64_t size_of_serialized_proto = value_to_serialize.ByteSizeLong();

    byte_size = size_of_object_size_block + size_of_serialized_proto;
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to serialize a protobuf message object into bytes buffer. The
   * format of the serialization will be [object_size][serialized_object].
   *
   * @param bytes_buffer The bytes buffer to serialize the object to.
   * @param buffer_offset The offset on the buffer to serialize the object to.
   * @param value_to_serialize The value of the object to serialize.
   * @param bytes_serialized The total bytes serialized after the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  template <typename TProtoMessage>
  static ExecutionResult SerializeProtoMessage(
      core::BytesBuffer& bytes_buffer, const size_t buffer_offset,
      const TProtoMessage& value_to_serialize, size_t& bytes_serialized) {
    bytes_serialized = 0;
    uint64_t size_of_serialized_proto = value_to_serialize.ByteSizeLong();
    if (buffer_offset + size_of_serialized_proto > bytes_buffer.capacity) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE);
    }

    // Serialize the actual proto object.
    if (!value_to_serialize.SerializeToArray(
            bytes_buffer.bytes->data() + buffer_offset,
            size_of_serialized_proto)) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_PROTO_SERIALIZATION_FAILED);
    }

    bytes_serialized = size_of_serialized_proto;
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to deserialize a protobuf message value from bytes buffer.
   *
   * @param bytes_buffer The bytes buffer to deserialize the object from.
   * @param buffer_offset The offset on the buffer to deserialize the object
   * from.
   * @param value_to_deserialize The value of the object to deserialize.
   * @param bytes_serialized The total bytes deserialized after the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  template <typename TProtoMessage>
  static ExecutionResult DeserializeProtoMessage(
      const core::BytesBuffer& bytes_buffer, const size_t buffer_offset,
      const size_t message_length, TProtoMessage& value_to_deserialize,
      size_t& bytes_deserialized) {
    bytes_deserialized = 0;

    if (buffer_offset + message_length > bytes_buffer.length) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
    }

    if (bytes_buffer.length == 0 ||
        !value_to_deserialize.ParseFromArray(
            bytes_buffer.bytes->data() + buffer_offset, message_length)) {
      return FailureExecutionResult(
          errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED);
    }

    bytes_deserialized = value_to_deserialize.ByteSizeLong();
    return SuccessExecutionResult();
  }

  /**
   * @brief Used to deserialize a protobuf message value from bytes buffer.
   *
   * @param bytes_buffer The bytes buffer to deserialize the object from.
   * @param buffer_offset The offset on the buffer to deserialize the object
   * from.
   * @param value_to_deserialize The value of the object to deserialize.
   * @param bytes_serialized The total bytes deserialized after the operation.
   * @return ExecutionResult The execution result of the operation.
   */
  template <typename TProtoMessage>
  static ExecutionResult DeserializeProtoMessage(
      const std::string& proto_message, TProtoMessage& value_to_deserialize,
      size_t& bytes_deserialized) {
    bytes_deserialized = 0;

    if (proto_message.length() == 0 ||
        !value_to_deserialize.ParseFromArray(proto_message.data(),
                                             proto_message.length())) {
      return FailureExecutionResult(
          core::errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED);
    }

    bytes_deserialized = proto_message.length();
    return SuccessExecutionResult();
  }

  template <typename TProtoMessage>
  static ExecutionResult ValidateVersion(const TProtoMessage& proto_message,
                                         Version supported_version) {
    if (proto_message.version().major() != supported_version.major ||
        proto_message.version().minor() != supported_version.minor) {
      return FailureExecutionResult(
          core::errors::SC_SERIALIZATION_VERSION_IS_INVALID);
    }

    return SuccessExecutionResult();
  }
};

/**
 * @brief Used to serialize a Uuid value into bytes buffer. The format of the
 * serialization will be [high_value][low_value].
 *
 * @param bytes_buffer The bytes buffer to serialize the object to.
 * @param buffer_offset The offset on the buffer to serialize the object to.
 * @param value_to_serialize The value of the object to serialize.
 * @param bytes_serialized The total bytes serialized after the operation.
 * @return ExecutionResult The execution result of the operation.
 */
template <>
inline ExecutionResult Serialization::Serialize<common::Uuid>(
    core::BytesBuffer& bytes_buffer, const size_t buffer_offset,
    const common::Uuid& value_to_serialize, size_t& bytes_serialized) {
  bytes_serialized = 0;
  size_t size_of_object_in_bytes = 2 * sizeof(uint64_t);
  if (buffer_offset + size_of_object_in_bytes > bytes_buffer.capacity) {
    return FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE);
  }

  // Serialize the high value.
  auto execution_result = Serialize(bytes_buffer, buffer_offset,
                                    value_to_serialize.high, bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Serialize the low value.
  execution_result = Serialize(bytes_buffer, buffer_offset + bytes_serialized,
                               value_to_serialize.low, bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  bytes_serialized = size_of_object_in_bytes;
  return SuccessExecutionResult();
}

/**
 * @brief Used to deserialize a Uuid value from bytes buffer.
 *
 * @param bytes_buffer The bytes buffer to deserialize the object from.
 * @param buffer_offset The offset on the buffer to deserialize the object
 * from.
 * @param value_to_deserialize The value of the object to deserialize.
 * @param bytes_serialized The total bytes deserialized after the operation.
 * @return ExecutionResult The execution result of the operation.
 */
template <>
inline ExecutionResult Serialization::Deserialize<common::Uuid>(
    const core::BytesBuffer& bytes_buffer, const size_t buffer_offset,
    common::Uuid& value_to_deserialize, size_t& bytes_deserialized) {
  bytes_deserialized = 0;
  size_t size_of_object_in_bytes = 2 * sizeof(uint64_t);
  if (buffer_offset + size_of_object_in_bytes > bytes_buffer.length) {
    return FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
  }

  // Deserialize the high value.
  auto execution_result =
      Deserialize(bytes_buffer, buffer_offset, value_to_deserialize.high,
                  bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Deserialize the low value.
  execution_result =
      Deserialize(bytes_buffer, buffer_offset + bytes_deserialized,
                  value_to_deserialize.low, bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  bytes_deserialized = size_of_object_in_bytes;
  return SuccessExecutionResult();
}

/**
 * @brief Used to serialize a version value into bytes buffer. The format of the
 * serialization will be [major_value][minor_value].
 *
 * @param bytes_buffer The bytes buffer to serialize the object to.
 * @param buffer_offset The offset on the buffer to serialize the object to.
 * @param value_to_serialize The value of the object to serialize.
 * @param bytes_serialized The total bytes serialized after the operation.
 * @return ExecutionResult The execution result of the operation.
 */
template <>
inline ExecutionResult Serialization::Serialize<Version>(
    core::BytesBuffer& bytes_buffer, const size_t buffer_offset,
    const Version& value_to_serialize, size_t& bytes_serialized) {
  bytes_serialized = 0;
  size_t size_of_object_in_bytes = 2 * sizeof(uint64_t);
  if (buffer_offset + size_of_object_in_bytes > bytes_buffer.capacity) {
    return FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE);
  }

  // Serialize the major value.
  auto execution_result = Serialize(bytes_buffer, buffer_offset,
                                    value_to_serialize.major, bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Serialize the minor value.
  execution_result = Serialize(bytes_buffer, buffer_offset + bytes_serialized,
                               value_to_serialize.minor, bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  bytes_serialized = size_of_object_in_bytes;
  return SuccessExecutionResult();
}

/**
 * @brief Used to deserialize a version value from bytes buffer.
 *
 * @param bytes_buffer The bytes buffer to deserialize the object from.
 * @param buffer_offset The offset on the buffer to deserialize the object
 * from.
 * @param value_to_deserialize The value of the object to deserialize.
 * @param bytes_serialized The total bytes deserialized after the operation.
 * @return ExecutionResult The execution result of the operation.
 */
template <>
inline ExecutionResult Serialization::Deserialize<Version>(
    const core::BytesBuffer& bytes_buffer, const size_t buffer_offset,
    Version& value_to_deserialize, size_t& bytes_deserialized) {
  bytes_deserialized = 0;
  size_t size_of_object_in_bytes = 2 * sizeof(uint64_t);
  if (buffer_offset + size_of_object_in_bytes > bytes_buffer.length) {
    return FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
  }

  // Deserialize the major value.
  auto execution_result =
      Deserialize(bytes_buffer, buffer_offset, value_to_deserialize.major,
                  bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Deserialize the minor value.
  execution_result =
      Deserialize(bytes_buffer, buffer_offset + bytes_deserialized,
                  value_to_deserialize.minor, bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  bytes_deserialized = size_of_object_in_bytes;
  return SuccessExecutionResult();
}

/**
 * @brief Used to serialize a protobuf any object into bytes buffer. The
 * format of the serialization will be [object_size][serialized_object].
 *
 * @param bytes_buffer The bytes buffer to serialize the object to.
 * @param buffer_offset The offset on the buffer to serialize the object to.
 * @param value_to_serialize The value of the object to serialize.
 * @param bytes_serialized The total bytes serialized after the operation.
 * @return ExecutionResult The execution result of the operation.
 */
template <>
inline ExecutionResult Serialization::Serialize<google::protobuf::Message>(
    core::BytesBuffer& bytes_buffer, const size_t buffer_offset,
    const google::protobuf::Message& value_to_serialize,
    size_t& bytes_serialized) {
  // To store the Any objects, we store the size in the first uint64_t size
  // and then read the next block.
  bytes_serialized = 0;
  size_t size_of_object_size_block = sizeof(uint64_t);
  uint64_t size_of_serialized_proto = value_to_serialize.ByteSizeLong();
  if (buffer_offset + size_of_object_size_block + size_of_serialized_proto >
      bytes_buffer.capacity) {
    return FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE);
  }

  // Serialize the object size header.
  auto execution_result = Serialize(bytes_buffer, buffer_offset,
                                    size_of_serialized_proto, bytes_serialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  // Serialize the actual proto object.
  if (!value_to_serialize.SerializeToArray(
          bytes_buffer.bytes->data() + buffer_offset + bytes_serialized,
          size_of_serialized_proto)) {
    return FailureExecutionResult(
        errors::SC_SERIALIZATION_PROTO_SERIALIZATION_FAILED);
  }

  bytes_serialized = size_of_object_size_block + size_of_serialized_proto;
  return SuccessExecutionResult();
}

/**
 * @brief Used to deserialize a protobuf any value from bytes buffer.
 *
 * @param bytes_buffer The bytes buffer to deserialize the object from.
 * @param buffer_offset The offset on the buffer to deserialize the object
 * from.
 * @param value_to_deserialize The value of the object to deserialize.
 * @param bytes_serialized The total bytes deserialized after the operation.
 * @return ExecutionResult The execution result of the operation.
 */
template <>
inline ExecutionResult Serialization::Deserialize<google::protobuf::Message>(
    const core::BytesBuffer& bytes_buffer, const size_t buffer_offset,
    google::protobuf::Message& value_to_deserialize,
    size_t& bytes_deserialized) {
  bytes_deserialized = 0;
  // Read the size block
  size_t size_of_object_size_block = sizeof(uint64_t);
  if (buffer_offset + size_of_object_size_block > bytes_buffer.length) {
    return FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
  }

  uint64_t size_of_serialized_proto = 0;
  auto execution_result =
      Deserialize(bytes_buffer, buffer_offset, size_of_serialized_proto,
                  bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (buffer_offset + size_of_object_size_block + size_of_serialized_proto >
      bytes_buffer.length) {
    return FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE);
  }

  if (size_of_serialized_proto != 0 &&
      !value_to_deserialize.ParseFromArray(
          bytes_buffer.bytes->data() + buffer_offset + bytes_deserialized,
          size_of_serialized_proto)) {
    return FailureExecutionResult(
        errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED);
  }

  bytes_deserialized = size_of_object_size_block + size_of_serialized_proto;
  return SuccessExecutionResult();
}

}  // namespace google::scp::core::common
