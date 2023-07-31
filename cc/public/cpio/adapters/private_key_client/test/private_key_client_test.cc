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

#include "public/cpio/adapters/private_key_client/src/private_key_client.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <gmock/gmock.h>

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/private_key_client_provider/mock/mock_private_key_client_provider_with_overrides.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/private_key_client/mock/mock_private_key_client_with_overrides.h"
#include "public/cpio/core/mock/mock_lib_cpio.h"
#include "public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::protobuf::MapPair;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::ListPrivateKeysByIdsRequest;
using google::scp::cpio::ListPrivateKeysByIdsResponse;
using google::scp::cpio::PrivateKeyClient;
using google::scp::cpio::PrivateKeyClientOptions;
using google::scp::cpio::mock::MockPrivateKeyClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

namespace google::scp::cpio::test {
class PrivateKeyClientTest : public ::testing::Test {
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
    auto private_key_client_options = make_shared<PrivateKeyClientOptions>();
    client_ = make_unique<MockPrivateKeyClientWithOverrides>(
        private_key_client_options);
  }

  PrivateKey CreatePrivateKey(const string& key_id, const string& public_key,
                              const string& private_key,
                              Timestamp expiration_time_in_ms) {
    PrivateKey key({.key_id = key_id,
                    .public_key = public_key,
                    .private_key = private_key,
                    .expiration_time_in_ms = expiration_time_in_ms});
    return key;
  }

  void AddPrivateKeyProto(
      cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse&
          response,
      const string& key_id, const string& public_key, const string& private_key,
      Timestamp expiration_time_in_ms) {
    auto key_proto = response.add_private_keys();
    key_proto->set_key_id(key_id);
    key_proto->set_public_key(public_key);
    key_proto->set_private_key(private_key);
    key_proto->set_expiration_time_in_ms(expiration_time_in_ms);
  }

  unique_ptr<MockPrivateKeyClientWithOverrides> client_;
};

MATCHER_P(EqualsKey, other, "") {
  return arg.key_id == other.key_id && arg.public_key == other.public_key &&
         arg.private_key == other.private_key &&
         arg.expiration_time_in_ms == other.expiration_time_in_ms;
}

TEST_F(PrivateKeyClientTest, ListSinglePrivateKeySuccess) {
  auto expected_result = SuccessExecutionResult();
  client_->GetPrivateKeyClientProvider()->list_private_keys_by_ids_result_mock =
      expected_result;

  cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest proto_request;
  string key_id = "key1";
  proto_request.add_key_ids(key_id);
  client_->GetPrivateKeyClientProvider()
      ->list_private_keys_by_ids_request_mock = proto_request;

  cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse
      proto_response;
  string public_key = "public_key";
  string private_key = "private_key";
  Timestamp expiration_time_in_ms = 1111;
  AddPrivateKeyProto(proto_response, key_id, public_key, private_key,
                     expiration_time_in_ms);
  client_->GetPrivateKeyClientProvider()
      ->list_private_keys_by_ids_response_mock = proto_response;

  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  atomic<bool> condition = false;
  ListPrivateKeysByIdsRequest request;
  request.key_ids.emplace_back(key_id);
  EXPECT_EQ(
      client_->ListPrivateKeysByIds(
          request,
          [&](const ExecutionResult result,
              ListPrivateKeysByIdsResponse response) {
            EXPECT_EQ(result, SuccessExecutionResult());
            EXPECT_THAT(
                response.private_keys,
                testing::UnorderedElementsAre(EqualsKey(CreatePrivateKey(
                    key_id, public_key, private_key, expiration_time_in_ms))));
            condition = true;
          }),
      SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(PrivateKeyClientTest, ListMultiplePrivateKeySuccess) {
  auto expected_result = SuccessExecutionResult();
  client_->GetPrivateKeyClientProvider()->list_private_keys_by_ids_result_mock =
      expected_result;

  cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest proto_request;
  string key_id_1 = "key1";
  string key_id_2 = "key2";
  proto_request.add_key_ids(key_id_1);
  proto_request.add_key_ids(key_id_2);
  client_->GetPrivateKeyClientProvider()
      ->list_private_keys_by_ids_request_mock = proto_request;

  cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse
      proto_response;
  string public_key_1 = "public_key_1";
  string private_key_1 = "private_key_1";
  Timestamp expiration_time_in_ms_1 = 1111;
  string public_key_2 = "public_key_2";
  string private_key_2 = "private_key_2";
  Timestamp expiration_time_in_ms_2 = 2222;
  AddPrivateKeyProto(proto_response, key_id_1, public_key_1, private_key_1,
                     expiration_time_in_ms_1);
  AddPrivateKeyProto(proto_response, key_id_2, public_key_2, private_key_2,
                     expiration_time_in_ms_2);
  client_->GetPrivateKeyClientProvider()
      ->list_private_keys_by_ids_response_mock = proto_response;

  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  atomic<bool> condition = false;
  ListPrivateKeysByIdsRequest request;
  request.key_ids.emplace_back(key_id_1);
  request.key_ids.emplace_back(key_id_2);
  EXPECT_EQ(client_->ListPrivateKeysByIds(
                request,
                [&](const ExecutionResult result,
                    ListPrivateKeysByIdsResponse response) {
                  EXPECT_EQ(result, SuccessExecutionResult());
                  EXPECT_THAT(response.private_keys,
                              testing::UnorderedElementsAre(
                                  EqualsKey(CreatePrivateKey(
                                      key_id_1, public_key_1, private_key_1,
                                      expiration_time_in_ms_1)),
                                  EqualsKey(CreatePrivateKey(
                                      key_id_2, public_key_2, private_key_2,
                                      expiration_time_in_ms_2))));
                  condition = true;
                }),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(PrivateKeyClientTest, ListPrivateKeysFailure) {
  auto expected_result = FailureExecutionResult(SC_UNKNOWN);
  client_->GetPrivateKeyClientProvider()->list_private_keys_by_ids_result_mock =
      expected_result;
  cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest proto_request;
  string key_id = "key1";
  proto_request.add_key_ids(key_id);
  client_->GetPrivateKeyClientProvider()
      ->list_private_keys_by_ids_request_mock = proto_request;

  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());
  atomic<bool> condition = false;

  ListPrivateKeysByIdsRequest request;
  request.key_ids.emplace_back(key_id);
  EXPECT_EQ(
      client_->ListPrivateKeysByIds(request,
                                    [&](const ExecutionResult result,
                                        ListPrivateKeysByIdsResponse response) {
                                      EXPECT_EQ(result, expected_result);
                                      condition = true;
                                    }),
      expected_result);
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(PrivateKeyClientTest, InitFailure) {
  auto expected_result = FailureExecutionResult(SC_UNKNOWN);
  client_->GetPrivateKeyClientProvider()->init_result_mock = expected_result;
  EXPECT_EQ(client_->Init(), expected_result);
}

TEST_F(PrivateKeyClientTest, RunFailure) {
  auto expected_result = FailureExecutionResult(SC_UNKNOWN);
  client_->GetPrivateKeyClientProvider()->run_result_mock = expected_result;
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), expected_result);
}

TEST_F(PrivateKeyClientTest, StopFailure) {
  auto expected_result = FailureExecutionResult(SC_UNKNOWN);
  client_->GetPrivateKeyClientProvider()->stop_result_mock = expected_result;
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());
  EXPECT_EQ(client_->Stop(), expected_result);
}
}  // namespace google::scp::cpio::test
