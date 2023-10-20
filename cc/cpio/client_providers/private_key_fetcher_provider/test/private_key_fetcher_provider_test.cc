
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

#include "cpio/client_providers/private_key_fetcher_provider/src/private_key_fetcher_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/private_key_fetcher_provider/mock/mock_private_key_fetcher_provider_with_overrides.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

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
    SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_DATA_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_RESOURCE_NAME_NOT_FOUND;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::PrivateKeyFetchingRequest;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using google::scp::cpio::client_providers::mock::
    MockPrivateKeyFetcherProviderWithOverrides;
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
class PrivateKeyFetcherProviderTest : public ::testing::Test {
 protected:
  PrivateKeyFetcherProviderTest()
      : http_client_(make_shared<MockHttpClient>()),
        private_key_fetcher_provider_(
            make_unique<MockPrivateKeyFetcherProviderWithOverrides>(
                http_client_)) {
    private_key_fetcher_provider_->signed_http_request_mock->path =
        make_shared<string>(string(kPrivateKeyBaseUri) + "/" + string(kKeyId));
    EXPECT_SUCCESS(private_key_fetcher_provider_->Init());
    EXPECT_SUCCESS(private_key_fetcher_provider_->Run());
    request_ = make_shared<PrivateKeyFetchingRequest>();
    request_->key_id = make_shared<string>(kKeyId);
    request_->key_vending_endpoint = make_shared<PrivateKeyVendingEndpoint>();
    request_->key_vending_endpoint->private_key_vending_service_endpoint =
        kPrivateKeyBaseUri;
    request_->key_vending_endpoint->service_region = kRegion;
  }

  ~PrivateKeyFetcherProviderTest() {
    if (private_key_fetcher_provider_) {
      EXPECT_SUCCESS(private_key_fetcher_provider_->Stop());
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
  unique_ptr<MockPrivateKeyFetcherProviderWithOverrides>
      private_key_fetcher_provider_;
  shared_ptr<PrivateKeyFetchingRequest> request_;
};

TEST_F(PrivateKeyFetcherProviderTest, MissingHttpClient) {
  private_key_fetcher_provider_ =
      make_unique<MockPrivateKeyFetcherProviderWithOverrides>(nullptr);

  EXPECT_THAT(private_key_fetcher_provider_->Init(),
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND)));
}

TEST_F(PrivateKeyFetcherProviderTest, FetchPrivateKey) {
  MockRequest(string(kPrivateKeyBaseUri) + "/" + kKeyId);
  MockResponse(
      R"({
    "name": "encryptionKeys/123456",
    "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
    "publicKeysetHandle": "primaryKeyId",
    "publicKeyMaterial": "testtest",
    "creationTime": "1669252790485",
    "expirationTime": "1669943990485",
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

  atomic<bool> condition = false;

  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      request_, [&](AsyncContext<PrivateKeyFetchingRequest,
                                 PrivateKeyFetchingResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->encryption_keys.size(), 1);
        const auto& encryption_key = *context.response->encryption_keys.begin();
        EXPECT_EQ(*encryption_key->resource_name, "encryptionKeys/123456");

        condition = true;
        return SuccessExecutionResult();
      });
  EXPECT_THAT(private_key_fetcher_provider_->FetchPrivateKey(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(PrivateKeyFetcherProviderTest, FailedToFetchPrivateKey) {
  ExecutionResult result = FailureExecutionResult(SC_UNKNOWN);
  http_client_->http_get_result_mock = result;

  atomic<bool> condition = false;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      move(request_), [&](AsyncContext<PrivateKeyFetchingRequest,
                                       PrivateKeyFetchingResponse>& context) {
        condition = true;
        EXPECT_THAT(context.result, ResultIs(result));
      });
  EXPECT_THAT(private_key_fetcher_provider_->FetchPrivateKey(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(PrivateKeyFetcherProviderTest, FailedToSignHttpRequest) {
  ExecutionResult result = FailureExecutionResult(SC_UNKNOWN);
  private_key_fetcher_provider_->sign_http_request_result_mock = result;

  atomic<bool> condition = false;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      move(request_), [&](AsyncContext<PrivateKeyFetchingRequest,
                                       PrivateKeyFetchingResponse>& context) {
        condition = true;
        EXPECT_THAT(context.result, ResultIs(result));
      });
  EXPECT_THAT(private_key_fetcher_provider_->FetchPrivateKey(context),
              ResultIs(result));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(PrivateKeyFetcherProviderTest, PrivateKeyNotFound) {
  MockRequest(string(kPrivateKeyBaseUri) + "/" + kKeyId);
  MockResponse(
      R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": "1669252790485",
        "expirationTime": "1669943990485",
        "ttlTime": 0
    })");

  atomic<bool> condition = false;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      move(request_), [&](AsyncContext<PrivateKeyFetchingRequest,
                                       PrivateKeyFetchingResponse>& context) {
        condition = true;
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_DATA_NOT_FOUND)));
      });
  EXPECT_THAT(private_key_fetcher_provider_->FetchPrivateKey(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
