
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

#include <google/protobuf/util/time_util.h>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/test/utils/timestamp_test_utils.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/kms_client_provider/mock/mock_kms_client_provider.h"
#include "cpio/client_providers/private_key_client_provider/mock/mock_private_key_client_provider_with_overrides.h"
#include "cpio/client_providers/private_key_client_provider/src/private_key_client_utils.h"
#include "cpio/client_providers/private_key_fetcher_provider/mock/mock_private_key_fetcher_provider.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::protobuf::Any;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ExpectTimestampEquals;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::mock::MockKmsClientProvider;
using google::scp::cpio::client_providers::mock::
    MockPrivateKeyClientProviderWithOverrides;
using google::scp::cpio::client_providers::mock::MockPrivateKeyFetcherProvider;
using std::atomic;
using std::make_pair;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::Between;
using testing::ElementsAre;
using testing::Pointwise;

static constexpr char kTestAccountIdentity1[] = "Test1";
static constexpr char kTestAccountIdentity2[] = "Test2";
static constexpr char kTestAccountIdentity3[] = "Test3";
static constexpr char kTestGcpWipProvider1[] = "Wip1";
static constexpr char kTestGcpWipProvider2[] = "Wip2";
static constexpr char kTestGcpWipProvider3[] = "Wip3";
static constexpr char kTestEndpoint1[] = "endpoint1";
static constexpr char kTestEndpoint2[] = "endpoint2";
static constexpr char kTestEndpoint3[] = "endpoint3";
static const vector<string> kTestEndpoints = {kTestEndpoint1, kTestEndpoint2,
                                              kTestEndpoint3};
static constexpr char kTestRegion1[] = "region1";
static constexpr char kTestRegion2[] = "region2";
static constexpr char kTestRegion3[] = "region3";
static const vector<string> kTestKeyIds = {"key_id_1", "key_id_2", "key_id_3"};
static constexpr char kTestKeyIdBad[] = "bad_key_id";
static constexpr char kTestResourceName[] = "encryptionKeys/key_id";
static constexpr char kTestPublicKeysetHandle[] = "publicKeysetHandle";
static constexpr char kTestPublicKeyMaterial[] = "publicKeyMaterial";
static constexpr int kTestExpirationTime = 123456;
static constexpr int kTestCreationTime = 111111;
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
static const map<string, ExecutionResult>
    kMockSuccessKeyFetchingResultsForListByAge = {
        {kTestEndpoint1, SuccessExecutionResult()},
        {kTestEndpoint2, SuccessExecutionResult()},
        {kTestEndpoint3, SuccessExecutionResult()}};

static const map<string, map<string, ExecutionResult>>
    kMockSuccessKeyFetchingResults = {
        {kTestKeyIds[0],
         {{kTestEndpoint1, SuccessExecutionResult()},
          {kTestEndpoint2, SuccessExecutionResult()},
          {kTestEndpoint3, SuccessExecutionResult()}}},
        {kTestKeyIds[1],
         {{kTestEndpoint1, SuccessExecutionResult()},
          {kTestEndpoint2, SuccessExecutionResult()},
          {kTestEndpoint3, SuccessExecutionResult()}}},
        {kTestKeyIds[2],
         {{kTestEndpoint1, SuccessExecutionResult()},
          {kTestEndpoint2, SuccessExecutionResult()},
          {kTestEndpoint3, SuccessExecutionResult()}}}};

static void GetPrivateKeyFetchingResponse(PrivateKeyFetchingResponse& response,
                                          int key_id_index, int uri_index,
                                          size_t splits_in_key_data = 3,
                                          bool bad_key_material = false) {
  auto encryption_key = make_shared<EncryptionKey>();
  encryption_key->resource_name = make_shared<string>(kTestResourceName);
  encryption_key->expiration_time_in_ms = kTestExpirationTime;
  encryption_key->creation_time_in_ms = kTestCreationTime;
  encryption_key->encryption_key_type =
      EncryptionKeyType::kMultiPartyHybridEvenKeysplit;
  encryption_key->public_key_material =
      make_shared<string>(kTestPublicKeyMaterial);
  encryption_key->public_keyset_handle =
      make_shared<string>(kTestPublicKeysetHandle);

  for (auto i = 0; i < splits_in_key_data; ++i) {
    auto key_data = make_shared<KeyData>();
    key_data->key_encryption_key_uri =
        make_shared<string>(kTestKeyEncryptionKeyUri);
    if (i == uri_index) {
      key_data->key_material = make_shared<string>(
          bad_key_material ? kTestKeyMaterialBad : kTestKeyMaterials[i]);
    }

    key_data->public_key_signature =
        make_shared<string>(kTestPublicKeySignature);
    encryption_key->key_data.emplace_back(key_data);
  }
  encryption_key->key_id = make_shared<string>(
      bad_key_material ? kTestKeyIdBad : kTestKeyIds[key_id_index]);
  response.encryption_keys.emplace_back(encryption_key);
}

static map<string, map<string, PrivateKeyFetchingResponse>>
CreateSuccessKeyFetchingResponseMap(size_t splits_in_key_data = 3,
                                    size_t call_num = 3) {
  map<string, map<string, PrivateKeyFetchingResponse>> responses;
  for (int i = 0; i < call_num; ++i) {
    for (int j = 0; j < 3; ++j) {
      PrivateKeyFetchingResponse mock_fetching_response;
      GetPrivateKeyFetchingResponse(mock_fetching_response, i, j,
                                    splits_in_key_data);
      responses[kTestKeyIds[i]][kTestEndpoints[j]] = mock_fetching_response;
    }
  }

  return responses;
}

static map<string, PrivateKeyFetchingResponse>
CreateSuccessKeyFetchingResponseMapForListByAge() {
  map<string, PrivateKeyFetchingResponse> responses;
  for (int i = 0; i < 3; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 3; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }

  return responses;
}

static const map<string, map<string, PrivateKeyFetchingResponse>>
    kMockSuccessKeyFetchingResponses = CreateSuccessKeyFetchingResponseMap();

class PrivateKeyClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    PrivateKeyVendingEndpoint endpoint_1;
    endpoint_1.account_identity = kTestAccountIdentity1;
    endpoint_1.gcp_wip_provider = kTestGcpWipProvider1;
    endpoint_1.service_region = kTestRegion1;
    endpoint_1.private_key_vending_service_endpoint = kTestEndpoint1;
    PrivateKeyVendingEndpoint endpoint_2;
    endpoint_2.account_identity = kTestAccountIdentity2;
    endpoint_2.gcp_wip_provider = kTestGcpWipProvider2;
    endpoint_2.service_region = kTestRegion2;
    endpoint_2.private_key_vending_service_endpoint = kTestEndpoint2;
    PrivateKeyVendingEndpoint endpoint_3;
    endpoint_3.account_identity = kTestAccountIdentity3;
    endpoint_3.gcp_wip_provider = kTestGcpWipProvider3;
    endpoint_3.service_region = kTestRegion3;
    endpoint_3.private_key_vending_service_endpoint = kTestEndpoint3;

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
    mock_private_key_fetcher =
        private_key_client_provider->GetPrivateKeyFetcherProvider();
    mock_kms_client = private_key_client_provider->GetKmsClientProvider();
    EXPECT_SUCCESS(private_key_client_provider->Init());
    EXPECT_SUCCESS(private_key_client_provider->Run());
  }

  void TearDown() override {
    if (private_key_client_provider) {
      EXPECT_SUCCESS(private_key_client_provider->Stop());
    }
  }

  void SetMockKmsClient(const ExecutionResult& mock_result, int8_t call_time) {
    EXPECT_CALL(*mock_kms_client, Decrypt)
        .Times(call_time)
        .WillRepeatedly(
            [=](AsyncContext<DecryptRequest, DecryptResponse>& context) {
              context.response = make_shared<DecryptResponse>();
              auto it = kPlaintextMap.find(context.request->ciphertext());
              if (it != kPlaintextMap.end()) {
                context.response->set_plaintext(it->second);
              }
              context.result = mock_result;
              context.Finish();
              return mock_result;
            });
  }

  void SetMockPrivateKeyFetchingClient(
      const map<string, map<string, ExecutionResult>>& mock_results,
      const map<string, map<string, PrivateKeyFetchingResponse>>&
          mock_responses) {
    EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
        .Times(Between(1, 9))
        .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                         PrivateKeyFetchingResponse>& context) {
          const auto& endpoint = context.request->key_vending_endpoint
                                     ->private_key_vending_service_endpoint;
          const auto& key_id = *context.request->key_id;
          context.result = mock_results.at(key_id).at(endpoint);
          if (context.result.Successful()) {
            if (mock_responses.find(key_id) != mock_responses.end() &&
                mock_responses.at(key_id).find(endpoint) !=
                    mock_responses.at(key_id).end()) {
              context.response = make_shared<PrivateKeyFetchingResponse>(
                  mock_responses.at(key_id).at(endpoint));
            }
          }
          context.Finish();
          return context.result;
        });
  }

  void SetMockPrivateKeyFetchingClientForListByAge(
      const map<string, ExecutionResult>& mock_results,
      const map<string, PrivateKeyFetchingResponse>& mock_responses) {
    EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
        .Times(Between(1, 9))
        .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                         PrivateKeyFetchingResponse>& context) {
          const auto& endpoint = context.request->key_vending_endpoint
                                     ->private_key_vending_service_endpoint;
          context.result = mock_results.at(endpoint);
          if (context.result.Successful()) {
            if (mock_responses.find(endpoint) != mock_responses.end()) {
              context.response = make_shared<PrivateKeyFetchingResponse>(
                  mock_responses.at(endpoint));
            }
          }
          context.Finish();
          return context.result;
        });
  }

  vector<PrivateKey> BuildExpectedPrivateKeys(const string& encoded_private_key,
                                              uint16_t start_index = 0,
                                              uint16_t end_index = 2) {
    vector<PrivateKey> expected_keys(end_index - start_index + 1);
    for (auto i = start_index; i <= end_index; ++i) {
      uint16_t key_index = i - start_index;
      expected_keys[key_index].set_key_id(kTestKeyIds[key_index]);
      expected_keys[key_index].set_public_key(kTestPublicKeyMaterial);
      expected_keys[key_index].set_private_key(encoded_private_key);
      *expected_keys[key_index].mutable_expiration_time() =
          TimeUtil::MillisecondsToTimestamp(kTestExpirationTime);
      *expected_keys[key_index].mutable_creation_time() =
          TimeUtil::MillisecondsToTimestamp(kTestCreationTime);
    }
    return expected_keys;
  }

  shared_ptr<MockPrivateKeyClientProviderWithOverrides>
      private_key_client_provider;
  shared_ptr<MockPrivateKeyFetcherProvider> mock_private_key_fetcher;
  shared_ptr<MockKmsClientProvider> mock_kms_client;
};

TEST_F(PrivateKeyClientProviderTest, ListPrivateKeysByIdsSuccess) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 9);

  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  kMockSuccessKeyFetchingResponses);
  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);
  request.add_key_ids(kTestKeyIds[2]);

  string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        auto expected_keys = BuildExpectedPrivateKeys(encoded_private_key);
        EXPECT_THAT(context.response->private_keys(),
                    Pointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.fetch_add(1);
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, ListPrivateKeysByAgeSuccess) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 9);

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge,
      CreateSuccessKeyFetchingResponseMapForListByAge());
  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        auto expected_keys = BuildExpectedPrivateKeys(encoded_private_key);
        EXPECT_THAT(context.response->private_keys(),
                    Pointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.fetch_add(1);
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, KeyListIsEmpty) {
  EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
      .Times(Between(1, 3))
      .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                       PrivateKeyFetchingResponse>& context) {
        context.result = SuccessExecutionResult();
        context.response = make_shared<PrivateKeyFetchingResponse>();
        context.Finish();
        return context.result;
      });

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 0);
        EXPECT_SUCCESS(context.result);
        response_count.fetch_add(1);
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, LastEndpointReturnEmptyList) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 6);

  map<string, PrivateKeyFetchingResponse> responses;
  for (int i = 0; i < 2; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 3; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }
  PrivateKeyFetchingResponse empty_response;
  responses[kTestEndpoints[2]] = empty_response;

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge, responses);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 0);
        EXPECT_SUCCESS(context.result);
        response_count.fetch_add(1);
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, LastEndpointMissingKeySplit) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 8);

  map<string, PrivateKeyFetchingResponse> responses;
  for (int i = 0; i < 2; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 3; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }
  PrivateKeyFetchingResponse empty_response;
  for (int j = 0; j < 2; ++j) {
    GetPrivateKeyFetchingResponse(empty_response, j, 2);
  }
  responses[kTestEndpoints[2]] = empty_response;

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge, responses);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 2);
        auto expected_keys =
            BuildExpectedPrivateKeys(encoded_private_key, 0, 1);
        EXPECT_THAT(context.response->private_keys(),
                    Pointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.fetch_add(1);
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, FirstEndpointMissingMultipleKeySplits) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 7);

  map<string, PrivateKeyFetchingResponse> responses;
  for (int i = 1; i < 3; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 3; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }
  PrivateKeyFetchingResponse empty_response;
  for (int j = 0; j < 1; ++j) {
    GetPrivateKeyFetchingResponse(empty_response, j, 0);
  }
  responses[kTestEndpoints[0]] = empty_response;

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge, responses);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 1);
        auto expected_keys =
            BuildExpectedPrivateKeys(encoded_private_key, 0, 0);
        EXPECT_THAT(context.response->private_keys(),
                    Pointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.fetch_add(1);
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest,
       IgnoreUnmatchedEndpointsAndKeyDataSplitsFailureForListByAge) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 7);

  map<string, PrivateKeyFetchingResponse> responses;
  for (int i = 1; i < 3; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 2; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }
  PrivateKeyFetchingResponse corrupted_response;
  for (int j = 0; j < 3; ++j) {
    GetPrivateKeyFetchingResponse(corrupted_response, j, 0);
  }
  corrupted_response.encryption_keys[0]->key_data.pop_back();
  responses[kTestEndpoints[0]] = corrupted_response;

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge, responses);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 2);
        auto expected_keys =
            BuildExpectedPrivateKeys(encoded_private_key, 0, 1);
        EXPECT_THAT(context.response->private_keys(),
                    Pointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.fetch_add(1);
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, FetchingPrivateKeysFailed) {
  PrivateKeyFetchingResponse mock_fetching_response;
  GetPrivateKeyFetchingResponse(mock_fetching_response, 0, 0);

  EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
      .Times(Between(1, 6))
      .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
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
      });

  // One private key obtain failed in the list will return failed
  // ListPrivateKeysResponse.
  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIdBad);

  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        response_count.fetch_add(1);

        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest,
       FailedWithUnmatchedEndpointsAndKeyDataSplits) {
  auto mock_fetching_responses = CreateSuccessKeyFetchingResponseMap(2);
  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  mock_fetching_responses);

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);
  request.add_key_ids(kTestKeyIds[2]);

  auto expected_result = FailureExecutionResult(
      SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS);
  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        response_count.fetch_add(1);
        EXPECT_THAT(context.result, ResultIs(expected_result));
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, FailedWithDecryptPrivateKey) {
  auto mock_result = FailureExecutionResult(SC_UNKNOWN);
  SetMockKmsClient(mock_result, 1);

  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  kMockSuccessKeyFetchingResponses);

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);
  request.add_key_ids(kTestKeyIds[2]);

  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        response_count.fetch_add(1);
        EXPECT_THAT(context.result, ResultIs(mock_result));
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}

TEST_F(PrivateKeyClientProviderTest, FailedWithOneKmsDecryptContext) {
  auto mock_result = FailureExecutionResult(SC_UNKNOWN);
  SetMockKmsClient(mock_result, 1);

  PrivateKeyFetchingResponse mock_fetching_response;
  GetPrivateKeyFetchingResponse(mock_fetching_response, 0, 0);
  PrivateKeyFetchingResponse mock_fetching_response_bad;
  GetPrivateKeyFetchingResponse(mock_fetching_response_bad, 1, 0, 3, true);

  EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
      .Times(Between(1, 6))
      .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
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
      });

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIdBad);

  atomic<size_t> response_count = 0;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        response_count.fetch_add(1);
        EXPECT_THAT(context.result, ResultIs(mock_result));
      });

  auto result = private_key_client_provider->ListPrivateKeys(context);
  EXPECT_SUCCESS(result);
  WaitUntil([&]() { return response_count.load() == 1; });
}
}  // namespace google::scp::cpio::client_providers::test
