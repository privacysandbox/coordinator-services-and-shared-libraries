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

#include "public/cpio/adapters/public_key_client/src/public_key_client.h"

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
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/public_key_client/mock/mock_public_key_client_with_overrides.h"
#include "public/cpio/core/mock/mock_lib_cpio.h"
#include "public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "public/cpio/interface/public_key_client/type_def.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::protobuf::MapPair;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::ListPublicKeysRequest;
using google::scp::cpio::ListPublicKeysResponse;
using google::scp::cpio::PublicKeyClient;
using google::scp::cpio::PublicKeyClientOptions;
using google::scp::cpio::mock::MockPublicKeyClientWithOverrides;
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
class PublicKeyClientTest : public ::testing::Test {
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
    auto public_key_client_options = make_shared<PublicKeyClientOptions>();
    client_ = make_unique<MockPublicKeyClientWithOverrides>(
        public_key_client_options);
  }

  PublicKey CreatePublicKey(const string& key_id, const string& public_key) {
    PublicKey key({.key_id = key_id, .public_key = public_key});
    return key;
  }

  void AddPublicKeyProto(
      cmrt::sdk::public_key_service::v1::ListPublicKeysResponse& response,
      const string& key_id, const string& public_key) {
    auto key_proto = response.add_public_keys();
    key_proto->set_key_id(key_id);
    key_proto->set_public_key(public_key);
  }

  unique_ptr<MockPublicKeyClientWithOverrides> client_;
};

MATCHER_P(EqualsKey, other, "") {
  return arg.key_id == other.key_id && arg.public_key == other.public_key;
}

TEST_F(PublicKeyClientTest, ListSinglePublicKeySuccess) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto expected_result = SuccessExecutionResult();
  client_->GetPublicKeyClientProvider()->list_public_keys_result_mock =
      expected_result;

  cmrt::sdk::public_key_service::v1::ListPublicKeysRequest proto_request;

  client_->GetPublicKeyClientProvider()->list_public_keys_request_mock =
      proto_request;

  cmrt::sdk::public_key_service::v1::ListPublicKeysResponse proto_response;
  string key_id = "key1";
  string public_key = "public_key";
  Timestamp expiration_time_in_ms = 1111;
  proto_response.set_expiration_time_in_ms(expiration_time_in_ms);
  AddPublicKeyProto(proto_response, key_id, public_key);
  client_->GetPublicKeyClientProvider()->list_public_keys_response_mock =
      proto_response;

  atomic<bool> condition = false;
  ListPublicKeysRequest request;
  EXPECT_EQ(
      client_->ListPublicKeys(
          request,
          [&](const ExecutionResult result, ListPublicKeysResponse response) {
            EXPECT_EQ(result, SuccessExecutionResult());
            EXPECT_THAT(response.public_keys,
                        testing::UnorderedElementsAre(
                            EqualsKey(CreatePublicKey(key_id, public_key))));
            EXPECT_EQ(response.expiration_time_in_ms, expiration_time_in_ms);
            condition = true;
          }),
      SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(PublicKeyClientTest, ListMultiplePublicKeySuccess) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto expected_result = SuccessExecutionResult();
  client_->GetPublicKeyClientProvider()->list_public_keys_result_mock =
      expected_result;

  cmrt::sdk::public_key_service::v1::ListPublicKeysRequest proto_request;

  client_->GetPublicKeyClientProvider()->list_public_keys_request_mock =
      proto_request;

  cmrt::sdk::public_key_service::v1::ListPublicKeysResponse proto_response;

  Timestamp expiration_time_in_ms = 1111;
  string public_key_1 = "public_key_1";
  string public_key_2 = "public_key_2";
  string key_id_1 = "key1";
  string key_id_2 = "key2";
  proto_response.set_expiration_time_in_ms(expiration_time_in_ms);
  AddPublicKeyProto(proto_response, key_id_1, public_key_1);
  AddPublicKeyProto(proto_response, key_id_2, public_key_2);
  client_->GetPublicKeyClientProvider()->list_public_keys_response_mock =
      proto_response;

  atomic<bool> condition = false;
  ListPublicKeysRequest request;
  EXPECT_EQ(
      client_->ListPublicKeys(
          request,
          [&](const ExecutionResult result, ListPublicKeysResponse response) {
            EXPECT_EQ(result, SuccessExecutionResult());
            EXPECT_THAT(
                response.public_keys,
                testing::UnorderedElementsAre(
                    EqualsKey(CreatePublicKey(key_id_1, public_key_1)),
                    EqualsKey(CreatePublicKey(key_id_2, public_key_2))));
            EXPECT_EQ(response.expiration_time_in_ms, expiration_time_in_ms);
            condition = true;
          }),
      SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(PublicKeyClientTest, ListPublicKeysFailure) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto expected_result = FailureExecutionResult(SC_UNKNOWN);
  client_->GetPublicKeyClientProvider()->list_public_keys_result_mock =
      expected_result;
  cmrt::sdk::public_key_service::v1::ListPublicKeysRequest proto_request;

  client_->GetPublicKeyClientProvider()->list_public_keys_request_mock =
      proto_request;

  atomic<bool> condition = false;

  ListPublicKeysRequest request;

  EXPECT_EQ(client_->ListPublicKeys(request,
                                    [&](const ExecutionResult result,
                                        ListPublicKeysResponse response) {
                                      EXPECT_EQ(result, expected_result);
                                      condition = true;
                                    }),
            expected_result);
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(PublicKeyClientTest, FailureToCreatePublicKeyClientProvider) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->create_public_key_client_provider_result = failure;
  EXPECT_EQ(client_->Init(), failure);
}

TEST_F(PublicKeyClientTest, RunFailure) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  auto expected_result = FailureExecutionResult(SC_UNKNOWN);
  client_->GetPublicKeyClientProvider()->run_result_mock = expected_result;
  EXPECT_EQ(client_->Run(), expected_result);
}

TEST_F(PublicKeyClientTest, StopFailure) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  auto expected_result = FailureExecutionResult(SC_UNKNOWN);
  client_->GetPublicKeyClientProvider()->stop_result_mock = expected_result;
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());
  EXPECT_EQ(client_->Stop(), expected_result);
}
}  // namespace google::scp::cpio::test
