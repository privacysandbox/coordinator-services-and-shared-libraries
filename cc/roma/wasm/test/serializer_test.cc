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

#include "roma/wasm/src/serializer.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "roma/wasm/src/wasm_types.h"

using std::string;
using std::vector;

namespace google::scp::roma::wasm::test {

using wasm::RomaWasmListOfStringRepresentation;

TEST(WasmSerializerTest, ShouldWriteUint8) {
  uint8_t mem_blob[4];
  memset(mem_blob, 0, sizeof(mem_blob));

  WasmSerializer::WriteUint8(mem_blob, sizeof(mem_blob), 0, 123);

  EXPECT_EQ(mem_blob[0], 123);
  EXPECT_EQ(mem_blob[1], 0);
  EXPECT_EQ(mem_blob[2], 0);
  EXPECT_EQ(mem_blob[3], 0);

  memset(mem_blob, 0, sizeof(mem_blob));

  EXPECT_TRUE(WasmSerializer::WriteUint8(mem_blob, sizeof(mem_blob), 2, 234));

  EXPECT_EQ(mem_blob[0], 0);
  EXPECT_EQ(mem_blob[1], 0);
  EXPECT_EQ(mem_blob[2], 234);
  EXPECT_EQ(mem_blob[3], 0);
}

TEST(WasmSerializerTest, ShouldNotWriteUint8IfOutOfBounds) {
  uint8_t mem_blob[4];
  memset(mem_blob, 0, sizeof(mem_blob));

  EXPECT_FALSE(WasmSerializer::WriteUint8(mem_blob, sizeof(mem_blob), 4, 123));
}

TEST(WasmSerializerTest, ShouldWriteUint16) {
  uint8_t mem_blob[4];
  memset(mem_blob, 0, sizeof(mem_blob));

  WasmSerializer::WriteUint16(mem_blob, sizeof(mem_blob), 0, 0x1234);

  // Write little endian
  EXPECT_EQ(mem_blob[0], 0x34);
  EXPECT_EQ(mem_blob[1], 0x12);
  EXPECT_EQ(mem_blob[2], 0);
  EXPECT_EQ(mem_blob[3], 0);

  memset(mem_blob, 0, sizeof(mem_blob));

  EXPECT_TRUE(
      WasmSerializer::WriteUint16(mem_blob, sizeof(mem_blob), 2, 0x4567));

  EXPECT_EQ(mem_blob[0], 0);
  EXPECT_EQ(mem_blob[1], 0);
  // Writes little endian
  EXPECT_EQ(mem_blob[2], 0x67);
  EXPECT_EQ(mem_blob[3], 0x45);
}

TEST(WasmSerializerTest, ShouldNotWriteUint16IfOutOfBounds) {
  uint8_t mem_blob[4];
  memset(mem_blob, 0, sizeof(mem_blob));

  EXPECT_FALSE(WasmSerializer::WriteUint16(mem_blob, sizeof(mem_blob), 3, 123));
}

TEST(WasmSerializerTest, ShouldWriteUint32) {
  uint8_t mem_blob[8];
  memset(mem_blob, 0, sizeof(mem_blob));

  WasmSerializer::WriteUint32(mem_blob, sizeof(mem_blob), 0, 0x12345678);

  // Write little endian
  EXPECT_EQ(mem_blob[0], 0x78);
  EXPECT_EQ(mem_blob[1], 0x56);
  EXPECT_EQ(mem_blob[2], 0x34);
  EXPECT_EQ(mem_blob[3], 0x12);
  EXPECT_EQ(mem_blob[4], 0);
  EXPECT_EQ(mem_blob[5], 0);
  EXPECT_EQ(mem_blob[6], 0);
  EXPECT_EQ(mem_blob[7], 0);

  memset(mem_blob, 0, sizeof(mem_blob));

  EXPECT_TRUE(
      WasmSerializer::WriteUint32(mem_blob, sizeof(mem_blob), 3, 0x456789AB));

  EXPECT_EQ(mem_blob[0], 0);
  EXPECT_EQ(mem_blob[1], 0);
  EXPECT_EQ(mem_blob[2], 0);
  EXPECT_EQ(mem_blob[3], 0xAB);
  EXPECT_EQ(mem_blob[4], 0x89);
  EXPECT_EQ(mem_blob[5], 0x67);
  EXPECT_EQ(mem_blob[6], 0x45);
  EXPECT_EQ(mem_blob[7], 0);
}

TEST(WasmSerializerTest, ShouldNotWriteUint32IfOutOfBounds) {
  uint8_t mem_blob[4];
  memset(mem_blob, 0, sizeof(mem_blob));

  EXPECT_FALSE(WasmSerializer::WriteUint32(mem_blob, sizeof(mem_blob), 1, 123));
}

TEST(WasmSerializerTest, ShouldWriteString) {
  uint8_t mem_blob[8];
  memset(mem_blob, 0, sizeof(mem_blob));

  EXPECT_TRUE(
      WasmSerializer::WriteRawString(mem_blob, sizeof(mem_blob), 2, "ABC", 3));

  EXPECT_EQ(mem_blob[0], 0);
  EXPECT_EQ(mem_blob[1], 0);
  EXPECT_EQ(mem_blob[2], 'A');
  EXPECT_EQ(mem_blob[3], 'B');
  EXPECT_EQ(mem_blob[4], 'C');
  EXPECT_EQ(mem_blob[5], 0);
  EXPECT_EQ(mem_blob[6], 0);
  EXPECT_EQ(mem_blob[7], 0);
}

TEST(WasmSerializerTest, ShouldNotWriteStringIfOutOfBounds) {
  uint8_t mem_blob[8];
  memset(mem_blob, 0, sizeof(mem_blob));

  EXPECT_FALSE(
      WasmSerializer::WriteRawString(mem_blob, sizeof(mem_blob), 6, "ABC", 3));
}

TEST(WasmSerializerTest, ShouldWriteCustomString) {
  uint8_t mem_blob[13];
  memset(mem_blob, 0xAA, sizeof(mem_blob));

  auto offset =
      WasmSerializer::WriteCustomString(mem_blob, sizeof(mem_blob), 1, "ABC");

  // The pointer to the beginning of the struct
  EXPECT_EQ(offset, 4);

  // Format in memory is inlined string data, followed by the pointer to the
  // data which is a uint32 (4 bytes), followed by the string size which is a
  // uint32 (4 bytes)

  // Bounds
  EXPECT_EQ(mem_blob[0], 0xAA);

  EXPECT_EQ(mem_blob[1], 'A');
  EXPECT_EQ(mem_blob[2], 'B');
  EXPECT_EQ(mem_blob[3], 'C');
  // Data pointer
  EXPECT_EQ(mem_blob[4], 0x01);
  EXPECT_EQ(mem_blob[5], 0);
  EXPECT_EQ(mem_blob[6], 0);
  EXPECT_EQ(mem_blob[7], 0);
  // String length
  EXPECT_EQ(mem_blob[8], 0x03);
  EXPECT_EQ(mem_blob[9], 0);
  EXPECT_EQ(mem_blob[10], 0);
  EXPECT_EQ(mem_blob[11], 0);
  // Bounds
  EXPECT_EQ(mem_blob[12], 0xAA);
}

TEST(WasmSerializerTest, ShouldNotWriteCustomStringIfOutOfBounds) {
  uint8_t mem_blob[11];
  memset(mem_blob, 0xAA, sizeof(mem_blob));

  // Does not fit by one
  auto ptr =
      WasmSerializer::WriteCustomString(mem_blob, sizeof(mem_blob), 1, "ABC");

  EXPECT_EQ(ptr, UINT32_MAX);
}

TEST(WasmSerializerTest, ShouldWriteCustomListString) {
  uint8_t mem_blob[25];
  memset(mem_blob, 0xAA, sizeof(mem_blob));

  vector<string> list = {"ABC"};

  auto offset = WasmSerializer::WriteCustomListOfString(
      mem_blob, sizeof(mem_blob), 1, list);

  // Layout in memory is inlined string representations, followed be a list of
  // string pointers (inlined uint32s), followed by the pointer to the list of
  // string pointers (uint32), followed by the size (uint32) of the list. This
  // is the pointer to the beginning of the struct representing the list of
  // strings.
  EXPECT_EQ(offset, 16);

  // Bounds
  EXPECT_EQ(mem_blob[0], 0xAA);

  // String mem layout
  EXPECT_EQ(mem_blob[1], 'A');
  EXPECT_EQ(mem_blob[2], 'B');
  EXPECT_EQ(mem_blob[3], 'C');
  EXPECT_EQ(mem_blob[4], 0x01);
  EXPECT_EQ(mem_blob[5], 0);
  EXPECT_EQ(mem_blob[6], 0);
  EXPECT_EQ(mem_blob[7], 0);
  EXPECT_EQ(mem_blob[8], 0x03);
  EXPECT_EQ(mem_blob[9], 0);
  EXPECT_EQ(mem_blob[10], 0);
  EXPECT_EQ(mem_blob[11], 0);
  // List of string pointers
  EXPECT_EQ(mem_blob[12], 0x04);
  EXPECT_EQ(mem_blob[13], 0);
  EXPECT_EQ(mem_blob[14], 0);
  EXPECT_EQ(mem_blob[15], 0);
  // Pointer to list of string pointer
  EXPECT_EQ(mem_blob[16], 0x0C);
  EXPECT_EQ(mem_blob[17], 0);
  EXPECT_EQ(mem_blob[18], 0);
  EXPECT_EQ(mem_blob[19], 0);
  // Size of the list
  EXPECT_EQ(mem_blob[20], 0x01);
  EXPECT_EQ(mem_blob[21], 0);
  EXPECT_EQ(mem_blob[22], 0);
  EXPECT_EQ(mem_blob[23], 0);

  // Bounds
  EXPECT_EQ(mem_blob[24], 0xAA);
}

TEST(WasmSerializerTest, ShouldNotWriteCustomListStringIfOutOfBounds) {
  uint8_t mem_blob[23];
  memset(mem_blob, 0, sizeof(mem_blob));

  vector<string> list = {"ABC"};

  EXPECT_EQ(23, RomaWasmListOfStringRepresentation::ComputeMemorySizeFor(list));

  // Fails by one
  auto offset = WasmSerializer::WriteCustomListOfString(
      mem_blob, sizeof(mem_blob), 1, list);

  EXPECT_EQ(UINT32_MAX, offset);
}
}  // namespace google::scp::roma::wasm::test
