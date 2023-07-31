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

#include "public/cpio/adapters/config_client/src/config_client.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <gmock/gmock.h>

#include "core/test/utils/conditional_wait.h"
#include "cpio/proto/config_client.pb.h"
#include "public/cpio/adapters/config_client/mock/mock_config_client_with_overrides.h"
#include "public/cpio/adapters/config_client/src/error_codes.h"
#include "public/cpio/core/mock/mock_lib_cpio.h"
#include "public/cpio/interface/config_client/config_client_interface.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CONFIG_CLIENT_INVALID_PARAMETER_NAME;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::ConfigClient;
using google::scp::cpio::GetInstanceIdRequest;
using google::scp::cpio::GetInstanceIdResponse;
using google::scp::cpio::GetParameterRequest;
using google::scp::cpio::GetParameterResponse;
using google::scp::cpio::GetTagRequest;
using google::scp::cpio::GetTagResponse;
using google::scp::cpio::config_client::GetInstanceIdProtoRequest;
using google::scp::cpio::config_client::GetInstanceIdProtoResponse;
using google::scp::cpio::config_client::GetTagProtoRequest;
using google::scp::cpio::config_client::GetTagProtoResponse;
using google::scp::cpio::mock::MockConfigClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;

static constexpr char kParameterName[] = "parameter_name";
static constexpr char kParameterValue[] = "parameter_value";
static constexpr char kTagName[] = "tag_name";
static constexpr char kTagValue[] = "tag_value";
static constexpr char kInstanceId[] = "instance_id";

namespace google::scp::cpio::test {
class ConfigClientTest : public ::testing::Test {
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
    auto config_client_options = make_shared<ConfigClientOptions>();
    client_ = make_unique<MockConfigClientWithOverrides>(config_client_options);
  }

  unique_ptr<MockConfigClientWithOverrides> client_;
};

// Tests for GetParameter.
class GetParameterTest : public ConfigClientTest {
 protected:
  void SetUp() override {
    ConfigClientTest::SetUp();

    request_.set_parameter_name(kParameterName);
    client_->GetConfigClientProvider()->get_parameter_request_mock = request_;

    response_.set_parameter_value(kParameterValue);
    client_->GetConfigClientProvider()->get_parameter_response_mock = response_;
  }

  void TearDown() override {
    request_.Clear();
    response_.Clear();
  }

  cmrt::sdk::parameter_service::v1::GetParameterRequest request_;
  cmrt::sdk::parameter_service::v1::GetParameterResponse response_;
  GetParameterResponse actual_output_;
};

TEST_F(GetParameterTest, EmptyParameterName) {
  GetParameterRequest input;
  atomic<bool> condition = false;
  EXPECT_EQ(
      client_->GetParameter(
          input, [&](const ExecutionResult result,
                     GetParameterResponse response) { condition = true; }),
      FailureExecutionResult(SC_CONFIG_CLIENT_INVALID_PARAMETER_NAME));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GetParameterTest, GetParameterFailure) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->GetConfigClientProvider()->get_parameter_result_mock = failure;
  GetParameterRequest input;
  input.parameter_name = kParameterName;
  atomic<bool> condition = false;
  EXPECT_EQ(client_->GetParameter(input,
                                  [&](const ExecutionResult result,
                                      GetParameterResponse response) {
                                    EXPECT_EQ(result, failure);
                                    condition = true;
                                  }),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GetParameterTest, GetParameterSuccessfully) {
  GetParameterRequest input;
  input.parameter_name = kParameterName;
  atomic<bool> condition = false;
  EXPECT_EQ(
      client_->GetParameter(
          input,
          [&](const ExecutionResult result, GetParameterResponse response) {
            EXPECT_EQ(result, SuccessExecutionResult());
            EXPECT_EQ(response.parameter_value, kParameterValue);
            condition = true;
          }),
      SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

// Tests for GetTag.
class GetTagTest : public ConfigClientTest {
 protected:
  void SetUp() override {
    ConfigClientTest::SetUp();

    request_.set_tag_name(kTagName);
    client_->GetConfigClientProvider()->get_tag_request_mock = request_;

    response_.set_value(kTagValue);
    client_->GetConfigClientProvider()->get_tag_response_mock = response_;
  }

  void TearDown() override {
    request_.Clear();
    response_.Clear();
  }

  GetTagProtoRequest request_;
  GetTagProtoResponse response_;
  GetTagResponse actual_output_;
};

TEST_F(GetTagTest, GetTagFailure) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->GetConfigClientProvider()->get_tag_result_mock = failure;

  atomic<bool> condition = false;
  GetTagRequest get_tag_request;
  get_tag_request.tag_name = kTagName;
  EXPECT_EQ(client_->GetTag(
                move(get_tag_request),
                [&](const ExecutionResult result, GetTagResponse response) {
                  EXPECT_EQ(result, failure);
                  condition = true;
                }),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GetTagTest, GetTagSuccessfully) {
  atomic<bool> condition = false;
  GetTagRequest get_tag_request;
  get_tag_request.tag_name = kTagName;
  EXPECT_EQ(client_->GetTag(
                move(get_tag_request),
                [&](const ExecutionResult result, GetTagResponse response) {
                  EXPECT_EQ(result, SuccessExecutionResult());
                  EXPECT_EQ(response.tag_value, kTagValue);
                  condition = true;
                }),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

// Tests for GetInstanceId.
class GetInstanceIdTest : public ConfigClientTest {
 protected:
  void SetUp() override {
    ConfigClientTest::SetUp();

    client_->GetConfigClientProvider()->get_instance_id_request_mock = request_;

    response_.set_instance_id(kInstanceId);
    client_->GetConfigClientProvider()->get_instance_id_response_mock =
        response_;
  }

  void TearDown() override {
    request_.Clear();
    response_.Clear();
  }

  GetInstanceIdProtoRequest request_;
  GetInstanceIdProtoResponse response_;
  GetInstanceIdResponse actual_output_;
};

TEST_F(GetInstanceIdTest, GetInstanceIdFailure) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->GetConfigClientProvider()->get_instance_id_result_mock = failure;

  atomic<bool> condition = false;
  EXPECT_EQ(client_->GetInstanceId(GetInstanceIdRequest(),
                                   [&](const ExecutionResult result,
                                       GetInstanceIdResponse response) {
                                     EXPECT_EQ(result, failure);
                                     condition = true;
                                   }),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GetInstanceIdTest, GetInstanceIdSuccessfully) {
  atomic<bool> condition = false;
  EXPECT_EQ(
      client_->GetInstanceId(
          GetInstanceIdRequest(),
          [&](const ExecutionResult result, GetInstanceIdResponse response) {
            EXPECT_EQ(result, SuccessExecutionResult());
            EXPECT_EQ(response.instance_id, kInstanceId);
            condition = true;
          }),
      SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::test
