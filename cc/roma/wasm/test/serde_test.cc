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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string.h>

#include <string>
#include <vector>

#include "roma/wasm/src/deserializer.h"
#include "roma/wasm/src/serializer.h"

using std::string;
using std::vector;

namespace google::scp::roma::wasm::test {
TEST(WasmSerDeTest, ShouldWriteAndReadCustomString) {
  uint8_t mem_blob[100];
  memset(mem_blob, 0, sizeof(mem_blob));

  string data = "Hello, I'm a string :)";
  auto ptr =
      WasmSerializer::WriteCustomString(mem_blob, sizeof(mem_blob), 0, data);

  string str;
  WasmDeserializer::ReadCustomString(mem_blob, sizeof(mem_blob), ptr, str);
  EXPECT_STREQ(data.c_str(), str.c_str());
  EXPECT_EQ(data.length(), str.length());
}

TEST(WasmSerDeTest, ShouldWriteAndReadCustomStringAndAllowEmptyString) {
  uint8_t mem_blob[10];
  memset(mem_blob, 0, sizeof(mem_blob));

  string data = "";
  auto ptr =
      WasmSerializer::WriteCustomString(mem_blob, sizeof(mem_blob), 0, data);

  string str;
  WasmDeserializer::ReadCustomString(mem_blob, sizeof(mem_blob), ptr, str);
  EXPECT_STREQ(data.c_str(), str.c_str());
  EXPECT_EQ(data.length(), str.length());
}

TEST(WasmSerDeTest, StringShouldManipulateTheRightOffsets) {
  uint8_t mem_blob[19];
  memset(mem_blob, 0, sizeof(mem_blob));

  // Take four bytes at the beginning of the memory segment
  WasmSerializer::WriteUint32(mem_blob, sizeof(mem_blob), 0, 0xF234567F);

  // Take four bytes at the end of the memory segment
  WasmSerializer::WriteUint32(mem_blob, sizeof(mem_blob), 15, 0xF234567F);

  string data = "ABC";
  // This should take 11 bytes of linear memory in this order:
  // 3 bytes for the string "ABC"
  // 4 bytes for the str data pointer
  // 4 bytes for the size_t representing the str length
  EXPECT_EQ(11, RomaWasmStringRepresentation::ComputeMemorySizeFor(data));

  auto ptr =
      WasmSerializer::WriteCustomString(mem_blob, sizeof(mem_blob), 4, data);

  EXPECT_EQ(0xF234567F,
            WasmDeserializer::ReadUint32(mem_blob, sizeof(mem_blob), 0));
  EXPECT_EQ(0xF234567F,
            WasmDeserializer::ReadUint32(mem_blob, sizeof(mem_blob), 15));

  string str;
  WasmDeserializer::ReadCustomString(mem_blob, sizeof(mem_blob), ptr, str);
  EXPECT_STREQ(data.c_str(), str.c_str());
  EXPECT_EQ(data.length(), str.length());
}

TEST(WasmSerDeTest, ShouldWriteAndReadCustomListOfString) {
  uint8_t mem_blob[255];
  memset(mem_blob, 0, sizeof(mem_blob));

  vector<string> list = {"hello", "we", "are", "strings"};

  auto ptr = WasmSerializer::WriteCustomListOfString(mem_blob, sizeof(mem_blob),
                                                     0, list);

  vector<string> read_list;
  WasmDeserializer::ReadCustomListOfString(mem_blob, sizeof(mem_blob), ptr,
                                           read_list);

  EXPECT_EQ(list.size(), read_list.size());
  ASSERT_THAT(read_list, testing::ElementsAre("hello", "we", "are", "strings"));
}

TEST(WasmSerDeTest, ShouldWriteAndReadCustomListOfStringAndAllowEmptyStrings) {
  uint8_t mem_blob[255];
  memset(mem_blob, 0, sizeof(mem_blob));

  vector<string> list = {"hello", "", "", "strings"};

  auto ptr = WasmSerializer::WriteCustomListOfString(mem_blob, sizeof(mem_blob),
                                                     0, list);

  vector<string> read_list;
  WasmDeserializer::ReadCustomListOfString(mem_blob, sizeof(mem_blob), ptr,
                                           read_list);

  EXPECT_EQ(list.size(), read_list.size());
  ASSERT_THAT(read_list, testing::ElementsAre("hello", "", "", "strings"));
}

TEST(WasmSerDeTest, ListOfStringShouldManipulateTheRightOffsets) {
  uint8_t mem_blob[46];
  memset(mem_blob, 0, sizeof(mem_blob));

  // Take four bytes at the beginning of the memory segment
  WasmSerializer::WriteUint32(mem_blob, sizeof(mem_blob), 0, 0xF234567F);

  // Take four bytes at the end of the memory segment
  WasmSerializer::WriteUint32(mem_blob, sizeof(mem_blob), 42, 0xF234567F);

  vector<string> data = {"ABC", "DEF"};
  // This should take 38 bytes of memory in this order:
  // 3 bytes for the string "ABC"
  // 4 bytes for the str data pointer
  // 4 bytes for the size_t representing the str length
  //---> 11
  // 3 bytes for the string "DEF"
  // 4 bytes for the str data pointer
  // 4 bytes for the size_t representing the str length
  //---> 11
  // 8 bytes for the pointers to the strings
  //---> 8
  // 4 bytes for the ptr to the list of string string pointers
  // 4 bytes for the size of the list
  //---> 8
  EXPECT_EQ(38, RomaWasmListOfStringRepresentation::ComputeMemorySizeFor(data));

  auto ptr = WasmSerializer::WriteCustomListOfString(mem_blob, sizeof(mem_blob),
                                                     4, data);

  EXPECT_EQ(0xF234567F,
            WasmDeserializer::ReadUint32(mem_blob, sizeof(mem_blob), 0));
  EXPECT_EQ(0xF234567F,
            WasmDeserializer::ReadUint32(mem_blob, sizeof(mem_blob), 42));

  vector<string> read_list;
  WasmDeserializer::ReadCustomListOfString(mem_blob, sizeof(mem_blob), ptr,
                                           read_list);

  EXPECT_EQ(data.size(), read_list.size());
  ASSERT_THAT(read_list, testing::ElementsAre("ABC", "DEF"));
}
}  // namespace google::scp::roma::wasm::test
