// Copyright 2024 Google LLC
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

#include "cc/core/utils/src/http.h"

#include <algorithm>
#include <string>

#include "cc/core/utils/src/error_codes.h"
#include "gtest/gtest.h"

namespace google::scp::core {

TEST(HttpTest, NullHeaders) {
  core::HttpHeaders null_headers;

  auto extraction_result = utils::ExtractRequestClaimedIdentity(null_headers);

  // Since headers are empty, the function should fail.
  EXPECT_FALSE(extraction_result.Successful());
  EXPECT_EQ(extraction_result.result().status_code,
            core::errors::SC_CORE_REQUEST_HEADER_NOT_FOUND);
}

TEST(HttpTest, HeaderNotFound) {
  core::HttpHeaders empty_headers;  // Empty header map.

  auto extraction_result = utils::ExtractRequestClaimedIdentity(empty_headers);

  // Since the requested header is not found, the function should fail.
  EXPECT_FALSE(extraction_result.Successful());
  EXPECT_EQ(extraction_result.result().status_code,
            core::errors::SC_CORE_REQUEST_HEADER_NOT_FOUND);
}

TEST(HttpTest, HeaderFound) {
  core::HttpHeaders request_headers;
  std::string claimed_identity = "claimed_identity";

  request_headers.insert(
      {std::string(core::kClaimedIdentityHeader), claimed_identity});

  auto extraction_result =
      utils::ExtractRequestClaimedIdentity(request_headers);

  // The function should succeed and return the correct value.
  EXPECT_TRUE(extraction_result.Successful());
  EXPECT_EQ(extraction_result.value(), claimed_identity);
}
}  // namespace google::scp::core
