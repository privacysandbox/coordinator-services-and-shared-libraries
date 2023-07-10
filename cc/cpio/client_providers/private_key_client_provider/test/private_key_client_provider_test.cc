
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

#include "cpio/client_providers/private_key_client_provider/src/private_key_client_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/core/Aws.h>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/kms_client_provider/mock/mock_kms_client_provider.h"
#include "cpio/client_providers/private_key_client_provider/mock/mock_private_key_client_provider_with_overrides.h"
#include "cpio/client_providers/private_key_client_provider/src/private_key_client_utils.h"
#include "cpio/client_providers/private_key_fetching_client_provider/mock/mock_private_key_fetching_client_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_test_lib.h"
#include "public/cpio/core/mock/mock_lib_cpio.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLIT_KEY_DATA;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::mock::MockKmsClientProvider;
using google::scp::cpio::client_providers::mock::
    MockPrivateKeyClientProviderWithOverrides;
using google::scp::cpio::client_providers::mock::
    MockPrivateKeyFetchingClientProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

static constexpr char kTestAccountIdentity1[] = "Test1";
static constexpr char kTestAccountIdentity2[] = "Test2";
static constexpr char kTestAccountIdentity3[] = "Test3";
static constexpr char kTestEndpoint1[] = "endpoint1";
static constexpr char kTestEndpoint2[] = "endpoint2";
static constexpr char kTestEndpoint3[] = "endpoint3";
static const vector<string> kTestEndpoints = {kTestEndpoint1, kTestEndpoint2,
                                              kTestEndpoint3};
static constexpr char kTestRegion1[] = "region1";
static constexpr char kTestRegion2[] = "region2";
static constexpr char kTestRegion3[] = "region3";
static constexpr char kTestKeyId[] = "key_id";
static constexpr char kTestKeyIdBad[] = "bad_key_id";
static constexpr char kTestResourceName[] = "encryptionKeys/key_id";
static constexpr char kTestPublicKeysetHandle[] = "publicKeysetHandle";
static constexpr char kTestPublicKeyMaterial[] = "publicKeyMaterial";
static constexpr int kTestExpirationTime = 123456;
static constexpr char kTestPublicKeySignature[] = "publicKeySignature";
static constexpr char kTestKeyEncryptionKeyUri[] = "keyEncryptionKeyUri";
static const vector<string> kTestKeyMaterials = {
    "key-material-1", "key-material-2", "key-material-3"};
static constexpr char kTestKeyMaterialBad[] = "bad-key-material";
static constexpr char kTestPrivateKey[] = "Test message";
static const map<string, string> kPlaintextMap = {
    {kTestKeyMaterials[0], "\270G\005\364$\253\273\331\353\336\216>"},
    {kTestKeyMaterials[1], "\327\002\204 \232\377\002\330\225DB\f"},
    {kTestKeyMaterials[2], "; \362\240\2369\334r\r\373\253W"}};

namespace google::scp::cpio::client_providers::test {
// Put them inside the namespace to use the type inside namespace easier.
static const map<string, ExecutionResult> kMockSuccessKeyFetchingResults = {
    {kTestEndpoint1, SuccessExecutionResult()},
    {kTestEndpoint2, SuccessExecutionResult()},
    {kTestEndpoint3, SuccessExecutionResult()}};

static void GetPrivateKeyFetchingResponse(PrivateKeyFetchingResponse& response,
                                          int split_index,
                                          size_t splits_in_key_data = 3,
                                          bool bad_key_material = false) {
  response.resource_name = make_shared<string>(kTestResourceName);
  response.expiration_time_ms = kTestExpirationTime;
  response.encryption_key_type =
      EncryptionKeyType::kMultiPartyHybridEvenKeysplit;
  response.public_key_material = make_shared<string>(kTestPublicKeyMaterial);
  response.public_keyset_handle = make_shared<string>(kTestPublicKeysetHandle);

  for (auto i = 0; i < splits_in_key_data; ++i) {
    auto key_data = make_shared<KeyData>();
    key_data->key_encryption_key_uri =
        make_shared<string>(kTestKeyEncryptionKeyUri);
    if (i == split_index) {
      if (bad_key_material) {
        key_data->key_material = make_shared<string>(kTestKeyMaterialBad);
      } else {
        key_data->key_material = make_shared<string>(kTestKeyMaterials[i]);
      }
    }

    key_data->public_key_signature =
        make_shared<string>(kTestPublicKeySignature);
    response.key_data.emplace_back(key_data);
  }
}

static map<string, PrivateKeyFetchingResponse>
CreateSuccessKeyFetchingResponseMap(size_t splits_in_key_data = 3,
                                    size_t splits_num = 3) {
  map<string, PrivateKeyFetchingResponse> responses;
  for (int i = 0; i < splits_num; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    GetPrivateKeyFetchingResponse(mock_fetching_response, i,
                                  splits_in_key_data);
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }

  return responses;
}

static const map<string, PrivateKeyFetchingResponse>
    kMockSuccessKeyFetchingResponses = CreateSuccessKeyFetchingResponseMap();

class PrivateKeyClientProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);

    InitCpio();
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);

    ShutdownCpio();
  }

  void SetUp() override {
    PrivateKeyVendingEndpoint endpoint_1(
        {.account_identity = kTestAccountIdentity1,
         .service_region = kTestRegion1,
         .private_key_vending_service_endpoint = kTestEndpoint1});
    PrivateKeyVendingEndpoint endpoint_2(
        {.account_identity = kTestAccountIdentity2,
         .service_region = kTestRegion2,
         .private_key_vending_service_endpoint = kTestEndpoint2});
    PrivateKeyVendingEndpoint endpoint_3(
        {.account_identity = kTestAccountIdentity3,
         .service_region = kTestRegion3,
         .private_key_vending_service_endpoint = kTestEndpoint3});

    auto private_key_client_options = make_shared<PrivateKeyClientOptions>();
    private_key_client_options->primary_private_key_vending_endpoint =
        endpoint_1;
    private_key_client_options->secondary_private_key_vending_endpoints
        .emplace_back(endpoint_2);
    private_key_client_options->secondary_private_key_vending_endpoints
        .emplace_back(endpoint_3);

    private_key_client_provider =
        make_shared<MockPrivateKeyClientProviderWithOverrides>(
            private_key_client_options);
    mock_private_key_fetching_client =
        private_key_client_provider->GetPrivateKeyFetchingClientProvider();
    mock_kms_client = private_key_client_provider->GetKmsClientProvider();
    EXPECT_THAT(private_key_client_provider->Init(), IsSuccessful());
    EXPECT_THAT(private_key_client_provider->Run(), IsSuccessful());
  }

  void TearDown() override {
    if (private_key_client_provider) {
      EXPECT_THAT(private_key_client_provider->Stop(), IsSuccessful());
    }
  }

  void SetMockKmsClient(const ExecutionResult& mock_result) {
    mock_kms_client->decrypt_mock =
        [mock_result](
            AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
          context.result = mock_result;
          if (mock_result.Successful()) {
            context.response = make_shared<KmsDecryptResponse>();
            context.response->plaintext = make_shared<string>(
                kPlaintextMap.at(*context.request->ciphertext));
          }
          context.Finish();
          return mock_result;
        };
  }

  void SetMockPrivateKeyFetchingClient(
      const map<string, ExecutionResult>& mock_results,
      const map<string, PrivateKeyFetchingResponse>& mock_responses) {
    mock_private_key_fetching_client->fetch_private_key_mock =
        [&](AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
                context) {
          auto endpoint = *context.request->private_key_service_base_uri;
          context.result = mock_results.at(endpoint);
          if (context.result.Successful()) {
            context.response = make_shared<PrivateKeyFetchingResponse>(
                mock_responses.at(endpoint));
          }
          context.Finish();
          return context.result;
        };
  }

  shared_ptr<MockPrivateKeyClientProviderWithOverrides>
      private_key_client_provider;
  shared_ptr<MockPrivateKeyFetchingClientProvider>
      mock_private_key_fetching_client;
  shared_ptr<MockKmsClientProvider> mock_kms_client;
};

TEST_F(PrivateKeyClientProviderTest, ListPrivateKeysByIdsSuccess) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result);

  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  kMockSuccessKeyFetchingResponses);
  ListPrivateKeysByIdsRequest request;
  request.add_key_ids(kTestKeyId);
  request.add_key_ids(kTestKeyId);
  request.add_key_ids(kTestKeyId);

  string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>
      context(make_shared<ListPrivateKeysByIdsRequest>(request),
              [&](AsyncContext<ListPrivateKeysByIdsRequest,
                               ListPrivateKeysByIdsResponse>& context) {
                response_count.fetch_add(1);
                for (auto i = 0; i < 3; ++i) {
                  auto key = context.response->private_keys()[i];
                  EXPECT_EQ(key.key_id(), kTestKeyId);
                  EXPECT_EQ(key.public_key(), kTestPublicKeyMaterial);
                  EXPECT_EQ(key.private_key(), encoded_private_key);
                  EXPECT_EQ(key.expiration_time_in_ms(), kTestExpirationTime);
                }
                EXPECT_THAT(context.result, IsSuccessful());
              });

  auto result = private_key_client_provider->ListPrivateKeysByIds(context);
  EXPECT_THAT(result, IsSuccessful());
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, ListPrivateKeysByIdsFailed) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result);

  PrivateKeyFetchingResponse mock_fetching_response;
  GetPrivateKeyFetchingResponse(mock_fetching_response, 0);
  mock_private_key_fetching_client->fetch_private_key_mock =
      [&](core::AsyncContext<PrivateKeyFetchingRequest,
                             PrivateKeyFetchingResponse>& context) {
        if (*context.request->key_id == kTestKeyIdBad) {
          context.result = FailureExecutionResult(SC_UNKNOWN);
          context.Finish();
          return SuccessExecutionResult();
        }

        context.response =
            make_shared<PrivateKeyFetchingResponse>(mock_fetching_response);
        context.result = SuccessExecutionResult();
        context.Finish();
        return context.result;
      };

  // One private key obtain failed in the list will return failed
  // ListPrivateKeysByIdsResponse.
  ListPrivateKeysByIdsRequest request;
  request.add_key_ids(kTestKeyId);
  request.add_key_ids(kTestKeyId);
  request.add_key_ids(kTestKeyIdBad);

  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>
      context(make_shared<ListPrivateKeysByIdsRequest>(request),
              [&](AsyncContext<ListPrivateKeysByIdsRequest,
                               ListPrivateKeysByIdsResponse>& context) {
                response_count.fetch_add(1);

                EXPECT_THAT(context.result,
                            ResultIs(FailureExecutionResult(SC_UNKNOWN)));
              });

  auto result = private_key_client_provider->ListPrivateKeysByIds(context);
  EXPECT_THAT(result, IsSuccessful());
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, FailedWithFetchPrivateKey) {
  SetMockKmsClient(SuccessExecutionResult());
  auto mock_failure_result = FailureExecutionResult(SC_UNKNOWN);
  map<string, ExecutionResult> mock_fetching_result = {
      {kTestEndpoint1, SuccessExecutionResult()},
      {kTestEndpoint2, SuccessExecutionResult()},
      {kTestEndpoint3, mock_failure_result}};
  auto mock_fetching_responses = CreateSuccessKeyFetchingResponseMap(3, 2);
  SetMockPrivateKeyFetchingClient(mock_fetching_result,
                                  mock_fetching_responses);

  ListPrivateKeysByIdsRequest request;
  request.add_key_ids(kTestKeyId);

  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>
      context(make_shared<ListPrivateKeysByIdsRequest>(request),
              [&](AsyncContext<ListPrivateKeysByIdsRequest,
                               ListPrivateKeysByIdsResponse>& context) {
                response_count.fetch_add(1);
                EXPECT_THAT(context.result, ResultIs(mock_failure_result));
              });

  auto result = private_key_client_provider->ListPrivateKeysByIds(context);
  EXPECT_THAT(result, ResultIs(mock_failure_result));
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest,
       FailedWithUnmatchedEndpointsAndKeyDataSplits) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result);
  auto mock_fetching_responses = CreateSuccessKeyFetchingResponseMap(2);
  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  mock_fetching_responses);

  ListPrivateKeysByIdsRequest request;
  request.add_key_ids(kTestKeyId);

  auto expected_result = FailureExecutionResult(
      SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLIT_KEY_DATA);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>
      context(make_shared<ListPrivateKeysByIdsRequest>(request),
              [&](AsyncContext<ListPrivateKeysByIdsRequest,
                               ListPrivateKeysByIdsResponse>& context) {
                response_count.fetch_add(1);
                EXPECT_THAT(context.result, ResultIs(expected_result));
              });

  auto result = private_key_client_provider->ListPrivateKeysByIds(context);
  EXPECT_THAT(result, ResultIs(mock_result));
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, FailedWithDecryptPrivateKey) {
  auto mock_result = FailureExecutionResult(SC_UNKNOWN);
  SetMockKmsClient(mock_result);

  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  kMockSuccessKeyFetchingResponses);

  ListPrivateKeysByIdsRequest request;
  request.add_key_ids(kTestKeyId);

  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>
      context(make_shared<ListPrivateKeysByIdsRequest>(request),
              [&](AsyncContext<ListPrivateKeysByIdsRequest,
                               ListPrivateKeysByIdsResponse>& context) {
                response_count.fetch_add(1);
                EXPECT_THAT(context.result, ResultIs(mock_result));
              });

  auto result = private_key_client_provider->ListPrivateKeysByIds(context);
  EXPECT_THAT(result, IsSuccessful());
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, FailedWithOneKmsDecryptContext) {
  auto mock_result = FailureExecutionResult(SC_UNKNOWN);
  mock_kms_client->decrypt_mock =
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        if (*context.request->ciphertext == kTestKeyMaterialBad) {
          context.result = mock_result;
          context.Finish();
          return SuccessExecutionResult();
        }
        KmsDecryptResponse kms_response;
        kms_response.plaintext =
            make_shared<string>(kPlaintextMap.at(*context.request->ciphertext));
        context.response = make_shared<KmsDecryptResponse>(kms_response);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      };

  PrivateKeyFetchingResponse mock_fetching_response;
  GetPrivateKeyFetchingResponse(mock_fetching_response, 0);
  PrivateKeyFetchingResponse mock_fetching_response_bad;
  GetPrivateKeyFetchingResponse(mock_fetching_response_bad, 1, 3, true);
  mock_private_key_fetching_client->fetch_private_key_mock =
      [&](core::AsyncContext<PrivateKeyFetchingRequest,
                             PrivateKeyFetchingResponse>& context) {
        if (*context.request->key_id == kTestKeyIdBad) {
          context.response = make_shared<PrivateKeyFetchingResponse>(
              mock_fetching_response_bad);
        } else {
          context.response =
              make_shared<PrivateKeyFetchingResponse>(mock_fetching_response);
        }

        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      };

  ListPrivateKeysByIdsRequest request;
  request.add_key_ids(kTestKeyId);
  request.add_key_ids(kTestKeyId);
  request.add_key_ids(kTestKeyIdBad);

  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>
      context(make_shared<ListPrivateKeysByIdsRequest>(request),
              [&](AsyncContext<ListPrivateKeysByIdsRequest,
                               ListPrivateKeysByIdsResponse>& context) {
                response_count.fetch_add(1);
                EXPECT_THAT(context.result, ResultIs(mock_result));
              });

  auto result = private_key_client_provider->ListPrivateKeysByIds(context);
  EXPECT_THAT(result, IsSuccessful());
  WaitUntil([&]() { return response_count.load() == 1; });
}

}  // namespace google::scp::cpio::client_providers::test
