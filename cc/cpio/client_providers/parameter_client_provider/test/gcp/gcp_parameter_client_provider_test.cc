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

#include <gtest/gtest.h>

#include <memory>

#include "absl/strings/str_cat.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/parameter_client_provider/mock/gcp/mock_gcp_parameter_client_provider_with_overrides.h"
#include "cpio/client_providers/parameter_client_provider/src/gcp/error_codes.h"
#include "cpio/common/src/gcp/error_codes.h"
#include "google/cloud/secretmanager/mocks/mock_secret_manager_connection.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

using absl::StrCat;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::StatusOr;
using google::cloud::secretmanager::SecretManagerServiceClient;
using google::cloud::secretmanager::v1::AccessSecretVersionRequest;
using google::cloud::secretmanager::v1::AccessSecretVersionResponse;
using google::cloud::secretmanager_mocks::MockSecretManagerServiceConnection;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncOperation;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::errors::SC_GCP_NOT_FOUND;
using google::scp::core::errors::
    SC_GCP_PARAMETER_CLIENT_PROVIDER_CREATE_SM_CLIENT_FAILURE;
using google::scp::core::errors::
    SC_GCP_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::core::errors::SC_GCP_UNKNOWN;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::
    MockGcpParameterClientProviderOverrides;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::NiceMock;
using testing::Return;

namespace {
constexpr char kInstanceResourceName[] =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr char kParameterNameMock[] = "parameter-name-test";
constexpr char kValueMock[] = "value";
constexpr char kProjectIdValueMock[] = "123456789";
}  // namespace

namespace google::scp::cpio::test {
class GcpParameterClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto async_executor_mock = make_shared<MockAsyncExecutor>();
    auto io_async_executor_mock = make_shared<MockAsyncExecutor>();

    auto instance_client_mock = make_shared<MockInstanceClientProvider>();
    instance_client_mock->instance_resource_name = kInstanceResourceName;

    client_ = make_unique<MockGcpParameterClientProviderOverrides>(
        async_executor_mock, io_async_executor_mock, instance_client_mock);

    connection_ = make_shared<NiceMock<MockSecretManagerServiceConnection>>();
    client_->secret_manager_mock =
        make_shared<SecretManagerServiceClient>(connection_);

    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());
  }

  void TearDown() override { EXPECT_SUCCESS(client_->Stop()); }

  string GetSecretName(const string& parameter_name = kParameterNameMock) {
    auto secret_name = StrCat("projects/", kProjectIdValueMock, "/secrets/",
                              parameter_name, "/versions/latest");

    return secret_name;
  }

  shared_ptr<MockSecretManagerServiceConnection> connection_;
  unique_ptr<MockGcpParameterClientProviderOverrides> client_;
};

MATCHER_P(RequestHasName, secret_name, "") {
  return ExplainMatchResult(Eq(secret_name), arg.name(), result_listener);
}

TEST_F(GcpParameterClientProviderTest, SucceedToFetchParameter) {
  auto secret_name_mock = GetSecretName();

  AccessSecretVersionResponse response;
  response.mutable_payload()->set_data(kValueMock);

  EXPECT_CALL(*connection_,
              AccessSecretVersion(RequestHasName(secret_name_mock)))
      .WillOnce(Return(response));

  atomic<bool> condition;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterNameMock);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->parameter_value(), kValueMock);
        condition = true;
      });

  EXPECT_SUCCESS(client_->GetParameter(context));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpParameterClientProviderTest, FailedToFetchParameterErrorNotFound) {
  auto secret_name_mock = GetSecretName();
  EXPECT_CALL(*connection_,
              AccessSecretVersion(RequestHasName(secret_name_mock)))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Not Found")));

  atomic<bool> condition;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterNameMock);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_NOT_FOUND)));
        condition = true;
      });

  EXPECT_SUCCESS(client_->GetParameter(context));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpParameterClientProviderTest, FailedWithInvalidParameterName) {
  auto request = make_shared<GetParameterRequest>();
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {});
  EXPECT_THAT(client_->GetParameter(context),
              ResultIs(FailureExecutionResult(
                  SC_GCP_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME)));
}

TEST_F(GcpParameterClientProviderTest,
       FailedToFetchParameterErrorInvalidArgument) {
  auto secret_name_mock = GetSecretName();
  EXPECT_CALL(*connection_,
              AccessSecretVersion(RequestHasName(secret_name_mock)))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "")));

  atomic<bool> condition;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterNameMock);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_INVALID_ARGUMENT)));
        condition = true;
      });

  EXPECT_SUCCESS(client_->GetParameter(context));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpParameterClientProviderTest, FailedToFetchParameterErrorUnknown) {
  auto secret_name_mock = GetSecretName();
  EXPECT_CALL(*connection_,
              AccessSecretVersion(RequestHasName(secret_name_mock)))
      .WillOnce(Return(Status(StatusCode::kUnknown, "")));

  atomic<bool> condition;
  auto request = make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterNameMock);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));
        condition = true;
      });

  EXPECT_SUCCESS(client_->GetParameter(context));
  WaitUntil([&]() { return condition.load(); });
}

TEST(GcpParameterClientProviderTestII, InitFailedToFetchProjectId) {
  auto async_executor_mock = make_shared<MockAsyncExecutor>();
  auto io_async_executor_mock = make_shared<MockAsyncExecutor>();
  auto instance_client_mock = make_shared<MockInstanceClientProvider>();
  instance_client_mock->get_instance_resource_name_mock =
      FailureExecutionResult(SC_UNKNOWN);

  auto connection = make_shared<NiceMock<MockSecretManagerServiceConnection>>();
  auto mock_sm_client = make_shared<SecretManagerServiceClient>(connection);

  auto client = make_unique<MockGcpParameterClientProviderOverrides>(
      async_executor_mock, io_async_executor_mock, instance_client_mock);

  EXPECT_THAT(client->Init(), ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST(GcpParameterClientProviderTestII, InitFailedToGetSMClient) {
  auto async_executor_mock = make_shared<MockAsyncExecutor>();
  auto io_async_executor_mock = make_shared<MockAsyncExecutor>();
  auto instance_client_mock = make_shared<MockInstanceClientProvider>();
  instance_client_mock->instance_resource_name = kInstanceResourceName;

  auto client = make_unique<MockGcpParameterClientProviderOverrides>(
      async_executor_mock, io_async_executor_mock, instance_client_mock);

  EXPECT_THAT(client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_GCP_PARAMETER_CLIENT_PROVIDER_CREATE_SM_CLIENT_FAILURE)));
}

}  // namespace google::scp::cpio::test
