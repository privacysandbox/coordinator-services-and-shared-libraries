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

#include "core/common/serialization/src/serialization.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "core/common/serialization/src/error_codes.h"
#include "core/common/serialization/test/test.pb.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/type_def.h"
#include "public/core/interface/execution_result.h"

using google::protobuf::Message;
using google::scp::core::common::Uuid;
using google::scp::core::common::test::serialization::TestStringRequest;
using std::make_shared;
using std::string;
using std::vector;

namespace google::scp::core::common::test {

template <typename T>
Byte GetNthByte(T& value, int n) {
  return (value >> (n * 8)) & 0xFF;
}

template <typename T>
void SerializeAndVerify(T value) {
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(sizeof(T));
  bytes_buffer.capacity = sizeof(T);
  bytes_buffer.length = 0;
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;

  EXPECT_EQ(Serialization::Serialize(bytes_buffer, buffer_offset, value,
                                     bytes_serialized),
            SuccessExecutionResult());
  EXPECT_EQ(bytes_buffer.bytes->size(), sizeof(T));
  EXPECT_EQ(bytes_buffer.capacity, sizeof(T));
  bytes_buffer.length += bytes_serialized;

  T deserialized;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(Serialization::Deserialize(bytes_buffer, buffer_offset,
                                       deserialized, bytes_deserialized),
            SuccessExecutionResult());

  EXPECT_EQ(value, deserialized);
  EXPECT_EQ(bytes_deserialized, bytes_serialized);

  bytes_buffer.capacity = 0;
  EXPECT_EQ(
      Serialization::Serialize(bytes_buffer, buffer_offset, value,
                               bytes_serialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));
  bytes_buffer.length = 0;
  EXPECT_EQ(
      Serialization::Deserialize(bytes_buffer, buffer_offset, value,
                                 bytes_deserialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
}

TEST(SerializationTests, SerializeChar) {
  SerializeAndVerify((int8_t)INT8_MIN);
  SerializeAndVerify((int8_t)-1);
  SerializeAndVerify((int8_t)0);
  SerializeAndVerify((int8_t)10);
  SerializeAndVerify((int8_t)INT8_MAX);
}

TEST(SerializationTests, SerializeUChar) {
  SerializeAndVerify((uint8_t)0);
  SerializeAndVerify((uint8_t)10);
  SerializeAndVerify((uint8_t)INT8_MAX);
}

TEST(SerializationTests, SerializeShort) {
  SerializeAndVerify((int16_t)INT16_MIN);
  SerializeAndVerify((int16_t)-1);
  SerializeAndVerify((int16_t)0);
  SerializeAndVerify((int16_t)10);
  SerializeAndVerify((int16_t)INT16_MAX);
}

TEST(SerializationTests, SerializeUShort) {
  SerializeAndVerify((uint16_t)0);
  SerializeAndVerify((uint16_t)10);
  SerializeAndVerify((uint16_t)UINT8_MAX);
}

TEST(SerializationTests, SerializeInt) {
  SerializeAndVerify((int32_t)INT_MIN);
  SerializeAndVerify((int32_t)-1);
  SerializeAndVerify((int32_t)0);
  SerializeAndVerify((int32_t)10);
  SerializeAndVerify((int32_t)INT_MAX);
}

TEST(SerializationTests, SerializeUInt) {
  SerializeAndVerify((uint32_t)0);
  SerializeAndVerify((uint32_t)10);
  SerializeAndVerify((uint32_t)UINT_MAX);
}

TEST(SerializationTests, SerializeInt64) {
  SerializeAndVerify((int64_t)INT64_MIN);
  SerializeAndVerify((int64_t)-1);
  SerializeAndVerify((int64_t)0);
  SerializeAndVerify((int64_t)10);
  SerializeAndVerify((int64_t)INT64_MAX);
}

TEST(SerializationTests, SerializeUInt64) {
  SerializeAndVerify((uint64_t)0);
  SerializeAndVerify((uint64_t)10);
  SerializeAndVerify((uint64_t)UINT64_MAX);
}

TEST(SerializationTests, SerializeUuid) {
  Uuid uuid;
  uuid.low = 1234;
  uuid.high = 5678;

  auto length = 2 * sizeof(uint64_t);
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(length);
  bytes_buffer.capacity = length;
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;
  EXPECT_EQ(Serialization::Serialize(bytes_buffer, buffer_offset, uuid,
                                     bytes_serialized),
            SuccessExecutionResult());

  EXPECT_EQ(bytes_buffer.bytes->size(), length);
  EXPECT_EQ(bytes_buffer.capacity, length);
  bytes_buffer.length += bytes_serialized;

  Uuid deserialized;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(Serialization::Deserialize(bytes_buffer, buffer_offset,
                                       deserialized, bytes_deserialized),
            SuccessExecutionResult());

  EXPECT_EQ(deserialized, uuid);
  EXPECT_EQ(bytes_deserialized, bytes_serialized);

  bytes_buffer.capacity = 0;
  EXPECT_EQ(
      Serialization::Serialize(bytes_buffer, buffer_offset, uuid,
                               bytes_serialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));
  bytes_buffer.length = 0;
  EXPECT_EQ(
      Serialization::Deserialize(bytes_buffer, buffer_offset, uuid,
                                 bytes_deserialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
}

TEST(SerializationTests, SerializeProto) {
  TestStringRequest test_string_request;
  test_string_request.set_request("test");

  auto block_header_length = sizeof(uint64_t);
  auto length = block_header_length + test_string_request.ByteSizeLong();
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(length);
  bytes_buffer.capacity = length;
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;

  EXPECT_EQ(
      Serialization::Serialize<Message>(bytes_buffer, buffer_offset,
                                        test_string_request, bytes_serialized),
      SuccessExecutionResult());

  EXPECT_EQ(bytes_buffer.bytes->size(), length);
  EXPECT_EQ(bytes_buffer.capacity, length);
  bytes_buffer.length += bytes_serialized;

  TestStringRequest test_string_request1;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(Serialization::Deserialize<Message>(bytes_buffer, buffer_offset,
                                                test_string_request1,
                                                bytes_deserialized),
            SuccessExecutionResult());

  EXPECT_EQ(test_string_request1.request(), test_string_request.request());
  EXPECT_EQ(bytes_deserialized, bytes_serialized);

  bytes_buffer.capacity = 0;
  EXPECT_EQ(
      Serialization::Serialize<Message>(bytes_buffer, buffer_offset,
                                        test_string_request1, bytes_serialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));
  bytes_buffer.length = 0;
  EXPECT_EQ(
      Serialization::Deserialize<Message>(bytes_buffer, buffer_offset,
                                          test_string_request1,
                                          bytes_deserialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
}

TEST(SerializationTests, SerializeProtoMessage) {
  TestStringRequest test_string_request;
  test_string_request.set_request("test");

  {
    size_t offset = 0;
    size_t bytes_serialized = 0;
    BytesBuffer bytes_buffer(1);
    EXPECT_EQ(
        Serialization::SerializeProtoMessage<TestStringRequest>(
            bytes_buffer, offset, test_string_request, bytes_serialized),
        FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));
  }

  {
    size_t offset = 0;
    size_t bytes_serialized = 0;
    BytesBuffer bytes_buffer(test_string_request.ByteSizeLong());
    EXPECT_EQ(Serialization::SerializeProtoMessage<TestStringRequest>(
                  bytes_buffer, offset, test_string_request, bytes_serialized),
              SuccessExecutionResult());
    EXPECT_EQ(bytes_serialized, test_string_request.ByteSizeLong());
  }

  {
    size_t offset = 0;
    size_t bytes_serialized = 0;
    BytesBuffer bytes_buffer(test_string_request.ByteSizeLong() + 1);
    bytes_buffer.length = 1;
    EXPECT_EQ(
        Serialization::SerializeProtoMessage<TestStringRequest>(
            bytes_buffer, offset + 1, test_string_request, bytes_serialized),
        SuccessExecutionResult());
    EXPECT_EQ(bytes_serialized, test_string_request.ByteSizeLong());
    bytes_buffer.length += bytes_serialized;

    size_t bytes_deserialized = 0;
    offset = 1;
    TestStringRequest test_string_request_deserialized;
    EXPECT_EQ(Serialization::DeserializeProtoMessage<TestStringRequest>(
                  bytes_buffer, offset, test_string_request.ByteSizeLong(),
                  test_string_request_deserialized, bytes_deserialized),
              SuccessExecutionResult());
    EXPECT_EQ(bytes_serialized, bytes_deserialized);
  }
}

TEST(SerializationTests, DeserializeProtoMessage) {
  TestStringRequest test_string_request;
  test_string_request.set_request("test");

  {
    size_t offset = 0;
    size_t bytes_deserialized = 0;
    BytesBuffer bytes_buffer(1);
    EXPECT_EQ(
        Serialization::DeserializeProtoMessage<TestStringRequest>(
            bytes_buffer, offset, 123, test_string_request, bytes_deserialized),
        FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
  }

  {
    size_t offset = 0;
    size_t bytes_deserialized = 0;
    BytesBuffer bytes_buffer(2000);
    bytes_buffer.length = 2000;
    EXPECT_EQ(
        Serialization::DeserializeProtoMessage<TestStringRequest>(
            bytes_buffer, offset, 123, test_string_request, bytes_deserialized),
        FailureExecutionResult(
            errors::SC_SERIALIZATION_PROTO_DESERIALIZATION_FAILED));
  }
}

TEST(SerializationTests, SerializeVersion) {
  Version version;
  version.major = 1234;
  version.minor = 5678;

  auto length = 2 * sizeof(uint64_t);
  BytesBuffer bytes_buffer;
  bytes_buffer.bytes = make_shared<vector<Byte>>(length);
  bytes_buffer.capacity = length;
  size_t buffer_offset = 0;
  size_t bytes_serialized = 0;
  EXPECT_EQ(Serialization::Serialize(bytes_buffer, buffer_offset, version,
                                     bytes_serialized),
            SuccessExecutionResult());

  EXPECT_EQ(bytes_buffer.bytes->size(), length);
  EXPECT_EQ(bytes_buffer.capacity, length);
  bytes_buffer.length += length;

  Version deserialized;
  size_t bytes_deserialized = 0;
  EXPECT_EQ(Serialization::Deserialize(bytes_buffer, buffer_offset,
                                       deserialized, bytes_deserialized),
            SuccessExecutionResult());

  EXPECT_EQ(deserialized, version);
  EXPECT_EQ(bytes_deserialized, bytes_serialized);

  bytes_buffer.capacity = 0;
  EXPECT_EQ(
      Serialization::Serialize(bytes_buffer, buffer_offset, version,
                               bytes_serialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_WRITABLE));
  bytes_buffer.length = 0;
  EXPECT_EQ(
      Serialization::Deserialize(bytes_buffer, buffer_offset, version,
                                 bytes_deserialized),
      FailureExecutionResult(errors::SC_SERIALIZATION_BUFFER_NOT_READABLE));
}

}  // namespace google::scp::core::common::test
