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

#include "core/utils/src/hashing.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "core/utils/src/error_codes.h"

using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using std::make_shared;
using std::string;
using std::vector;

namespace google::scp::core::utils::test {
TEST(HashingTest, InvalidMD5Hash) {
  BytesBuffer empty(0);
  empty.length = 0;

  string md5_hash;
  EXPECT_EQ(CalculateMd5Hash(empty, md5_hash),
            FailureExecutionResult(errors::SC_CORE_UTILS_INVALID_INPUT));
  EXPECT_EQ(md5_hash, "");
}

TEST(HashingTest, ValidMD5Hash) {
  BytesBuffer bytes_buffer;
  string value("this_is_a_test_string");
  bytes_buffer.bytes = make_shared<vector<Byte>>(value.begin(), value.end());
  bytes_buffer.length = value.length();

  string md5_hash;
  EXPECT_EQ(CalculateMd5Hash(bytes_buffer, md5_hash), SuccessExecutionResult());
  EXPECT_EQ(md5_hash, "!\x87\x9D\x8C\x7Fy\x93j\xCD\xB6\xE2\x86&\xEA\x1B\xD8");
}

}  // namespace google::scp::core::utils::test
