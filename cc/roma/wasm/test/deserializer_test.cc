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

#include "roma/wasm/src/deserializer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace google::scp::roma::wasm::test {
TEST(WasmDeserializerTest, ShouldReadUint8) {
  uint8_t mem_blob[4] = {4, 5, 6, 7};

  uint8_t read_val = WasmDeserializer::ReadUint8(mem_blob, sizeof(mem_blob), 0);
  EXPECT_EQ(read_val, 4);

  read_val = WasmDeserializer::ReadUint8(mem_blob, sizeof(mem_blob), 2);
  EXPECT_EQ(read_val, 6);
}

TEST(WasmDeserializerTest, ShouldNotReadUint8IfOffsetIsOutOfBounds) {
  uint8_t mem_blob[1] = {100};

  uint8_t read_val = WasmDeserializer::ReadUint8(mem_blob, sizeof(mem_blob), 1);
  EXPECT_EQ(read_val, 0);
}

TEST(WasmSerializerTest, ShouldReadUint16) {
  uint8_t mem_blob[4] = {0x78, 0x56, 0x34, 0x12};

  uint16_t read_val =
      WasmDeserializer::ReadUint16(mem_blob, sizeof(mem_blob), 1);
  EXPECT_EQ(read_val, 0x3456);

  read_val = WasmDeserializer::ReadUint16(mem_blob, sizeof(mem_blob), 2);
  EXPECT_EQ(read_val, 0x1234);
}

TEST(WasmDeserializerTest, ShouldNotReadUint16IfOffsetIsOutOfBounds) {
  uint8_t mem_blob[2] = {0x34, 0x12};

  uint8_t read_val =
      WasmDeserializer::ReadUint16(mem_blob, sizeof(mem_blob), 1);
  EXPECT_EQ(read_val, 0);
}

TEST(WasmSerializerTest, ShouldReadUint32) {
  uint8_t mem_blob[6] = {0x00, 0x78, 0x56, 0x34, 0x12, 0x00};

  uint32_t read_val =
      WasmDeserializer::ReadUint32(mem_blob, sizeof(mem_blob), 1);
  EXPECT_EQ(read_val, 0x12345678);

  read_val = WasmDeserializer::ReadUint32(mem_blob, sizeof(mem_blob), 0);
  EXPECT_EQ(read_val, 0x34567800);
}

TEST(WasmDeserializerTest, ShouldNotReadUint32IfOffsetIsOutOfBounds) {
  uint8_t mem_blob[4] = {0x78, 0x56, 0x34, 0x12};

  uint8_t read_val =
      WasmDeserializer::ReadUint32(mem_blob, sizeof(mem_blob), 1);
  EXPECT_EQ(read_val, 0);
}

TEST(WasmSerializerTest, ShouldReadRawString) {
  uint8_t mem_blob[9] = {0, 0, 0, 'r', 'o', 'm', 'a', 0, 0};

  char read_val[5];
  memset(read_val, 0, sizeof(read_val));
  WasmDeserializer::ReadRawString(mem_blob, sizeof(mem_blob), 3, &read_val[0],
                                  4);

  EXPECT_STREQ("roma", read_val);
}

TEST(WasmDeserializerTest, ShouldNotReadRawStringIfOffsetIsOutOfBounds) {
  uint8_t mem_blob[4] = {'r', 'o', 'm', 'a'};

  char read_val[5];
  memset(read_val, 0, sizeof(read_val));
  WasmDeserializer::ReadRawString(mem_blob, sizeof(mem_blob), 1, &read_val[0],
                                  4);

  EXPECT_STREQ("", read_val);
}

TEST(WasmDeserializerTest, ShouldReadCustomString) {
  // Format in memory is inlined string data, followed by the pointer to the
  // data which is a uint32 (4 bytes), followed by the string size which is a
  // uint32 (4 bytes)
  uint8_t mem_blob[] = {0xFF, 'r',
                        'o',  'm',
                        'a',  /* ptr to string data*/ 0x01,
                        0x00, 0x00,
                        0x00, /* string length */ 0x04,
                        0x00, 0x00,
                        0x00, 0xFF};

  string read_value;
  WasmDeserializer::ReadCustomString(mem_blob, sizeof(mem_blob), 5, read_value);

  EXPECT_EQ(read_value, "roma");
  EXPECT_EQ(read_value.length(), 4);
}

TEST(WasmDeserializerTest, ShouldNotReadCustomStringIfOffsetIsOutOfBounds) {
  uint8_t mem_blob[] = {0};

  string read_value;
  WasmDeserializer::ReadCustomString(mem_blob, sizeof(mem_blob), 1, read_value);

  EXPECT_EQ(read_value, "");
  EXPECT_EQ(read_value.length(), 0);
}

TEST(WasmDeserializerTest,
     ShouldNotReadCustomStringIfPointerToDataIsOutOfBounds) {
  uint8_t mem_blob[] = {0,    'r',
                        'o',  'm',
                        'a',  /* ptr to string data*/ 0x0D,  // Bad pointer
                        0x00, 0x00,
                        0x00, /* string length */ 0x04,
                        0x00, 0x00,
                        0x00};

  string read_value;
  WasmDeserializer::ReadCustomString(mem_blob, sizeof(mem_blob), 5, read_value);

  EXPECT_EQ(read_value, "");
  EXPECT_EQ(read_value.length(), 0);
}

TEST(WasmDeserializerTest, ShouldReadCustomListofStrings) {
  // Layout in memory is inlined string representations, followed be a list of
  // string pointers (inlined uint32s), followed by the pointer to the list of
  // string pointers (uint32), followed by the size (uint32) of the list. This
  // is the pointer to the beginning of the struct representing the list of
  // strings.
  uint8_t mem_blob[] = {0,
                        'r',
                        'o',
                        'm',
                        'a',
                        /* ptr to string data*/ 0x01,
                        0x00,
                        0x00,
                        0x00,
                        /* string length */ 0x04,
                        0x00,
                        0x00,
                        0x00,
                        /* inlined list of string pointers */ 0x05,
                        0x00,
                        0x00,
                        0x00,
                        /* ptr to list of string pointers */ 0x0D,
                        0x00,
                        0x00,
                        0x00,
                        /* size of the list */ 0x01,
                        0x00,
                        0x00,
                        0x00};

  vector<string> read_value;
  WasmDeserializer::ReadCustomListOfString(mem_blob, sizeof(mem_blob), 17,
                                           read_value);

  ASSERT_THAT(read_value, testing::ElementsAre("roma"));
}

TEST(WasmDeserializerTest, ShouldNoteReadCustomListofStringsIfOutOfBounds) {
  uint8_t mem_blob[10] = {};

  vector<string> read_value;
  // We can't even read the struct metadata starting at this index
  WasmDeserializer::ReadCustomListOfString(mem_blob, sizeof(mem_blob), 5,
                                           read_value);

  ASSERT_TRUE(read_value.empty());
}
}  // namespace google::scp::roma::wasm::test
