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

#include <string>

#include "cc/core/utils/src/error_codes.h"
#include "gtest/gtest.h"

namespace privacy_sandbox::pbs_common {
namespace {

TEST(HttpTest, NullHeadersExtractRequestClaimedIdentity) {
  HttpHeaders null_headers;

  ExecutionResultOr<std::string> extraction_result =
      ExtractRequestClaimedIdentity(null_headers);

  // Since headers are empty, the function should fail.
  EXPECT_FALSE(extraction_result.Successful());
  EXPECT_EQ(extraction_result.result().status_code,
            SC_CORE_REQUEST_HEADER_NOT_FOUND);
}

TEST(HttpTest, HeaderNotFoundExtractRequestClaimedIdentity) {
  HttpHeaders empty_headers;  // Empty header map.

  ExecutionResultOr<std::string> extraction_result =
      ExtractRequestClaimedIdentity(empty_headers);

  // Since the requested header is not found, the function should fail.
  EXPECT_FALSE(extraction_result.Successful());
  EXPECT_EQ(extraction_result.result().status_code,
            SC_CORE_REQUEST_HEADER_NOT_FOUND);
}

TEST(HttpTest, HeaderFoundExtractRequestClaimedIdentity) {
  HttpHeaders request_headers;
  std::string claimed_identity = "claimed_identity";

  request_headers.insert({"x-gscp-claimed-identity", claimed_identity});

  ExecutionResultOr<std::string> extraction_result =
      ExtractRequestClaimedIdentity(request_headers);

  // The function should succeed and return the correct value.
  EXPECT_TRUE(extraction_result.Successful());
  EXPECT_EQ(*extraction_result, claimed_identity);
}

TEST(HttpTest, NullHeadersExtractUserAgent) {
  HttpHeaders null_headers;  // Create empty headers.

  ExecutionResultOr<std::string> extraction_result =
      ExtractUserAgent(null_headers);

  // Since headers are empty, the function should fail.
  EXPECT_FALSE(extraction_result.Successful());
  EXPECT_EQ(extraction_result.result().status_code,
            SC_CORE_REQUEST_HEADER_NOT_FOUND);
}

TEST(HttpTest, HeaderNotFoundExtractUserAgent) {
  HttpHeaders empty_headers;  // Create an empty header map.

  ExecutionResultOr<std::string> extraction_result =
      ExtractUserAgent(empty_headers);

  // Since the User-Agent header is not found, the function should fail.
  EXPECT_FALSE(extraction_result.Successful());
  EXPECT_EQ(extraction_result.result().status_code,
            SC_CORE_REQUEST_HEADER_NOT_FOUND);
}

TEST(HttpTest, ValidUserAgentHeader) {
  HttpHeaders request_headers;

  request_headers.insert({"user-agent", "aggregation-service/2.5.0"});

  ExecutionResultOr<std::string> extraction_result =
      ExtractUserAgent(request_headers);

  // The function should succeed and return the correct value.
  EXPECT_TRUE(extraction_result.Successful());
  EXPECT_EQ(*extraction_result, "aggregation-service/2.5.0");

  request_headers.clear();
  request_headers.insert({"user-agent",
                          "aggregation-service/2.5.0 "
                          "(Commit/e8f289218a72b5008a30571cebdd2590c7eb0136)"});

  extraction_result = ExtractUserAgent(request_headers);

  // The function should succeed and return the correct value.
  EXPECT_TRUE(extraction_result.Successful());
  EXPECT_EQ(*extraction_result, "aggregation-service/2.5.0");
}

TEST(HttpTest, InvalidUserAgentFormat) {
  HttpHeaders request_headers;
  std::string invalid_user_agent = "some-other-service/2.5.0";

  request_headers.insert({"user-agent", invalid_user_agent});

  ExecutionResultOr<std::string> extraction_result =
      ExtractUserAgent(request_headers);

  // The function should succeed but return the unknown value.
  EXPECT_TRUE(extraction_result.Successful());
  EXPECT_EQ(*extraction_result, kUnknownValue);

  invalid_user_agent = "aggregation-service/2.5";
  request_headers.clear();
  request_headers.insert({"user-agent", invalid_user_agent});

  extraction_result = ExtractUserAgent(request_headers);

  EXPECT_TRUE(extraction_result.Successful());
  EXPECT_EQ(*extraction_result, kUnknownValue);

  invalid_user_agent =
      "aggregation-service/2.5 "
      "(Commit/e8f289218a72b5008a30571cebdd2590c7eb0136)";
  request_headers.clear();
  request_headers.insert({"user-agent", invalid_user_agent});

  extraction_result = ExtractUserAgent(request_headers);

  EXPECT_TRUE(extraction_result.Successful());
  EXPECT_EQ(*extraction_result, kUnknownValue);
}

TEST(HttpTest, EmptyUserAgentString) {
  HttpHeaders request_headers;
  request_headers.insert({"user-agent", ""});  // Empty User-Agent.

  ExecutionResultOr<std::string> extraction_result =
      ExtractUserAgent(request_headers);

  // The function should succeed and return the unknown value.
  EXPECT_TRUE(extraction_result.Successful());
  EXPECT_EQ(*extraction_result, kUnknownValue);
}

}  // namespace
}  // namespace privacy_sandbox::pbs_common
