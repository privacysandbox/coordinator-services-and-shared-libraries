
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

#include "cpio/client_providers/private_key_fetching_client_provider/src/private_key_fetching_client_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/private_key_fetching_client_provider/mock/mock_private_key_fetching_client_provider_with_overrides.h"
#include "cpio/client_providers/private_key_fetching_client_provider/src/error_codes.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::AsyncContext;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_RESOURCE_NAME_NOT_FOUND;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::PrivateKeyFetchingRequest;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using google::scp::cpio::client_providers::mock::
    MockPrivateKeyFetchingClientProviderWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

static constexpr char kKeyId[] = "123";
static constexpr char kRegion[] = "region";
static constexpr char kPrivateKeyBaseUri[] = "http://private_key/privateKeys";

namespace google::scp::cpio::client_providers::test {
class PrivateKeyFetchingClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    http_client_ = make_shared<MockHttpClient>();
    private_key_fetching_client_provider =
        make_unique<MockPrivateKeyFetchingClientProviderWithOverrides>(
            http_client_);
    EXPECT_EQ(private_key_fetching_client_provider->Init(),
              SuccessExecutionResult());
  }

  void TearDown() override {
    if (private_key_fetching_client_provider) {
      EXPECT_EQ(private_key_fetching_client_provider->Stop(),
                SuccessExecutionResult());
    }
  }

  void MockRequest(const string& uri) {
    http_client_->request_mock = HttpRequest();
    http_client_->request_mock.path = make_shared<string>(uri);
  }

  void MockResponse(const string& str) {
    BytesBuffer bytes_buffer(sizeof(str));
    bytes_buffer.bytes = make_shared<vector<Byte>>(str.begin(), str.end());
    bytes_buffer.capacity = sizeof(str);

    http_client_->response_mock = HttpResponse();
    http_client_->response_mock.body = move(bytes_buffer);
  }

  shared_ptr<MockHttpClient> http_client_;
  unique_ptr<MockPrivateKeyFetchingClientProviderWithOverrides>
      private_key_fetching_client_provider;
};

TEST_F(PrivateKeyFetchingClientProviderTest, Run) {
  EXPECT_EQ(private_key_fetching_client_provider->Run(),
            SuccessExecutionResult());
}

TEST_F(PrivateKeyFetchingClientProviderTest, MissingHttpClient) {
  private_key_fetching_client_provider =
      make_unique<MockPrivateKeyFetchingClientProviderWithOverrides>(nullptr);

  EXPECT_EQ(private_key_fetching_client_provider->Init().status_code,
            SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_HTTP_CLIENT_NOT_FOUND);
}

TEST_F(PrivateKeyFetchingClientProviderTest, FetchPrivateKey) {
  MockRequest(string(kPrivateKeyBaseUri) + "/" + kKeyId);
  MockResponse(
      R"({
    "name": "encryptionKeys/123456",
    "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
    "publicKeysetHandle": "primaryKeyId",
    "publicKeyMaterial": "testtest",
    "creationTime": 1669252790485,
    "expirationTime": 1669943990485,
    "ttlTime": 0,
    "keyData": [
        {
            "publicKeySignature": "",
            "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
            "keyMaterial": "test=test"
        },
        {
            "publicKeySignature": "",
            "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
            "keyMaterial": ""
        }
    ]
  })");

  auto request = make_shared<PrivateKeyFetchingRequest>();
  request->key_id = make_shared<string>(kKeyId);
  request->private_key_service_base_uri = make_shared<Uri>(kPrivateKeyBaseUri);
  request->service_region = make_shared<string>(kRegion);
  atomic<bool> condition = false;

  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      request, [&](AsyncContext<PrivateKeyFetchingRequest,
                                PrivateKeyFetchingResponse>& context) {
        EXPECT_EQ(context.result, SuccessExecutionResult());
        EXPECT_EQ(*context.response->resource_name, "encryptionKeys/123456");
        EXPECT_EQ(context.response->encryption_key_type,
                  EncryptionKeyType::kMultiPartyHybridEvenKeysplit);
        EXPECT_EQ(*context.response->public_keyset_handle, "primaryKeyId");
        EXPECT_EQ(*context.response->public_key_material, "testtest");
        EXPECT_EQ(context.response->expiration_time_ms, 1669943990485);
        EXPECT_EQ(*context.response->key_data[0]->key_encryption_key_uri,
                  "aws-kms://arn:aws:kms:us-east-1:1234567:key");
        EXPECT_EQ(*context.response->key_data[0]->public_key_signature, "");
        EXPECT_EQ(*context.response->key_data[0]->key_material, "test=test");
        EXPECT_EQ(*context.response->key_data[1]->key_encryption_key_uri,
                  "aws-kms://arn:aws:kms:us-east-1:12345:key");
        EXPECT_EQ(*context.response->key_data[1]->public_key_signature, "");
        EXPECT_EQ(*context.response->key_data[1]->key_material, "");

        condition = true;
        return SuccessExecutionResult();
      });

  EXPECT_EQ(private_key_fetching_client_provider->FetchPrivateKey(context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(PrivateKeyFetchingClientProviderTest, FailedToFetchPrivateKey) {
  ExecutionResult result = FailureExecutionResult(SC_UNKNOWN);
  http_client_->http_get_result_mock = result;

  auto request = make_shared<PrivateKeyFetchingRequest>();
  request->key_id = make_shared<string>(kKeyId);
  request->private_key_service_base_uri = make_shared<Uri>(kPrivateKeyBaseUri);
  request->service_region = make_shared<string>(kRegion);
  atomic<bool> condition = false;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      move(request), [&](AsyncContext<PrivateKeyFetchingRequest,
                                      PrivateKeyFetchingResponse>& context) {
        condition = true;
        EXPECT_EQ(context.result, result);
      });
  EXPECT_EQ(private_key_fetching_client_provider->FetchPrivateKey(context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(PrivateKeyFetchingClientProviderTest, FailedToSignHttpRequest) {
  ExecutionResult result = FailureExecutionResult(SC_UNKNOWN);
  private_key_fetching_client_provider->sign_http_request_result_mock = result;

  auto request = make_shared<PrivateKeyFetchingRequest>();
  request->key_id = make_shared<string>(kKeyId);
  request->private_key_service_base_uri = make_shared<Uri>(kPrivateKeyBaseUri);
  request->service_region = make_shared<string>(kRegion);
  atomic<bool> condition = false;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      move(request), [&](AsyncContext<PrivateKeyFetchingRequest,
                                      PrivateKeyFetchingResponse>& context) {
        condition = true;
        EXPECT_EQ(context.result, result);
      });
  EXPECT_EQ(private_key_fetching_client_provider->FetchPrivateKey(context),
            result);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(PrivateKeyFetchingClientProviderTest, PrivateKeyNotFound) {
  MockRequest(string(kPrivateKeyBaseUri) + "/" + kKeyId);
  MockResponse(
      R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": 1669252790485,
        "expirationTime": 1669943990485,
        "ttlTime": 0
    })");

  auto request = make_shared<PrivateKeyFetchingRequest>();
  request->key_id = make_shared<string>(kKeyId);
  request->private_key_service_base_uri = make_shared<Uri>(kPrivateKeyBaseUri);
  request->service_region = make_shared<string>(kRegion);
  atomic<bool> condition = false;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      move(request), [&](AsyncContext<PrivateKeyFetchingRequest,
                                      PrivateKeyFetchingResponse>& context) {
        condition = true;
        EXPECT_EQ(
            context.result,
            FailureExecutionResult(
                SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND));
      });
  EXPECT_EQ(private_key_fetching_client_provider->FetchPrivateKey(context),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
