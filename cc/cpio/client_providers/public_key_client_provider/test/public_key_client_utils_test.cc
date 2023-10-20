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

#include "cpio/client_providers/public_key_client_provider/src/public_key_client_utils.h"

#include <gtest/gtest.h>

#include <locale>
#include <memory>
#include <regex>
#include <utility>

#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::Uri;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED;
using google::scp::core::test::ResultIs;
using std::get_time;
using std::istringstream;
using std::mktime;
using std::regex;
using std::regex_replace;
using std::stoi;
using std::string;
using std::tm;
using std::vector;

static constexpr char kPublicKeyHeaderDate[] = "date";
static constexpr char kPublicKeyHeaderCacheControl[] = "cache-control";
static constexpr char kHeaderDateExample[] = "Wed, 16 Nov 2022 00:02:48 GMT";
static constexpr char kCacheControlExample[] = "max-age=254838";
static constexpr char kHeaderDateExampleBadStr[] = "2011-Februar-18 23:12:34";
static constexpr char kCacheControlExampleBadInt[] = "max-age=b2t54838abs";
static constexpr uint64_t kExpectedExpiredTimeSecs = 1668811806;

namespace google::scp::cpio::client_providers::test {
TEST(PublicKeyClientUtilsTest, ParseExpiredTimeFromHeadersSuccess) {
  HttpHeaders headers;
  headers.insert({kPublicKeyHeaderDate, kHeaderDateExample});
  headers.insert({kPublicKeyHeaderCacheControl, kCacheControlExample});

  uint64_t expired_time;
  auto result =
      PublicKeyClientUtils::ParseExpiredTimeFromHeaders(headers, expired_time);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(expired_time, kExpectedExpiredTimeSecs);
}

TEST(PublicKeyClientUtilsTest, HeadersMissDate) {
  HttpHeaders headers;
  headers.insert({kPublicKeyHeaderCacheControl, kCacheControlExample});

  uint64_t expired_time;
  auto result =
      PublicKeyClientUtils::ParseExpiredTimeFromHeaders(headers, expired_time);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED)));
}

TEST(PublicKeyClientUtilsTest, HeadersMissCacheControl) {
  HttpHeaders headers;
  headers.insert({kPublicKeyHeaderDate, kHeaderDateExample});

  uint64_t expired_time;
  auto result =
      PublicKeyClientUtils::ParseExpiredTimeFromHeaders(headers, expired_time);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED)));
}

TEST(PublicKeyClientUtilsTest, HeadersWithBadDateStr) {
  HttpHeaders headers;
  headers.insert({kPublicKeyHeaderDate, kHeaderDateExampleBadStr});
  headers.insert({kPublicKeyHeaderCacheControl, kCacheControlExample});

  uint64_t expired_time;
  auto result =
      PublicKeyClientUtils::ParseExpiredTimeFromHeaders(headers, expired_time);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED)));
}

TEST(PublicKeyClientUtilsTest, HeadersWithBadCacheControlStr) {
  HttpHeaders headers;
  headers.insert({kPublicKeyHeaderDate, kHeaderDateExample});
  headers.insert({kPublicKeyHeaderCacheControl, kCacheControlExampleBadInt});

  uint64_t expired_time;
  auto result =
      PublicKeyClientUtils::ParseExpiredTimeFromHeaders(headers, expired_time);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED)));
}

TEST(PublicKeyClientUtilsTest, ParsePublicKeysFromBodySuccess) {
  string bytes_str =
      "{\"keys\": [{\"id\": \"1234\", \"key\": \"abcdefg\"}, "
      "{\"id\": \"5678\", \"key\": \"hijklmn\"}]}";
  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  vector<PublicKey> public_keys;
  auto result =
      PublicKeyClientUtils::ParsePublicKeysFromBody(bytes, public_keys);

  EXPECT_SUCCESS(result);
  EXPECT_EQ(public_keys.size(), 2);
  EXPECT_EQ(public_keys[0].key_id(), "1234");
  EXPECT_EQ(public_keys[0].public_key(), "abcdefg");
  EXPECT_EQ(public_keys[1].key_id(), "5678");
  EXPECT_EQ(public_keys[1].public_key(), "hijklmn");
}

TEST(PublicKeyClientUtilsTest, ParsePublicKeysFromBodyNoKeys) {
  string bytes_str =
      "{\"key\": [{\"id\": \"1234\", \"key\": \"abcdefg\"}, "
      "{\"id\": \"5678\", \"key\": \"hijklmn\"}]}";
  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  vector<PublicKey> public_keys;
  auto result =
      PublicKeyClientUtils::ParsePublicKeysFromBody(bytes, public_keys);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED)));
}

TEST(PublicKeyClientUtilsTest, ParsePublicKeysFromBodyNoId) {
  string bytes_str =
      "{\"keys\": [{\"id_error\": \"1234\", \"key\": \"abcdefg\"}, "
      "{\"id\": \"5678\", \"key\": \"hijklmn\"}]}";
  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  vector<PublicKey> public_keys;
  auto result =
      PublicKeyClientUtils::ParsePublicKeysFromBody(bytes, public_keys);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED)));
}

TEST(PublicKeyClientUtilsTest, ParsePublicKeysFromBodyNoKey) {
  string bytes_str =
      "{\"keys\": [{\"id\": \"1234\", \"key_error\": \"abcdefg\"}, "
      "{\"id\": \"5678\", \"key\": \"hijklmn\"}]}";
  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  vector<PublicKey> public_keys;
  auto result =
      PublicKeyClientUtils::ParsePublicKeysFromBody(bytes, public_keys);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED)));
}

}  // namespace google::scp::cpio::client_providers::test
