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

#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/ec2/EC2Client.h>
#include <aws/ec2/model/DescribeInstancesRequest.h>
#include <aws/ec2/model/DescribeTagsRequest.h>

#include "absl/strings/str_format.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/curl_client/mock/mock_curl_client.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/auth_token_provider/mock/mock_auth_token_provider.h"
#include "cpio/client_providers/instance_client_provider/mock/aws/mock_ec2_client.h"
#include "cpio/client_providers/instance_client_provider/src/aws/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Client::AWSError;
using Aws::EC2::EC2Client;
using Aws::EC2::EC2Errors;
using Aws::EC2::Model::DescribeInstancesOutcome;
using Aws::EC2::Model::DescribeInstancesRequest;
using Aws::EC2::Model::DescribeInstancesResponse;
using Aws::EC2::Model::DescribeTagsOutcome;
using Aws::EC2::Model::DescribeTagsRequest;
using Aws::EC2::Model::DescribeTagsResponse;
using Aws::EC2::Model::Instance;
using Aws::EC2::Model::Reservation;
using Aws::EC2::Model::Tag;
using Aws::EC2::Model::TagDescription;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::protobuf::MapPair;
using google::scp::core::AsyncContext;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_INSTANCE_RESOURCE_NAME_RESPONSE_MALFORMED;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::errors::SC_AWS_INSTANCE_CLIENT_INVALID_REGION_CODE;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_DESCRIBE_INSTANCES_RESPONSE_MALFORMED;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::MockCurlClient;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::GetSessionTokenRequest;
using google::scp::cpio::client_providers::GetSessionTokenResponse;
using google::scp::cpio::client_providers::mock::MockAuthTokenProvider;
using google::scp::cpio::client_providers::mock::MockEC2Client;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::TestWithParam;
using testing::UnorderedElementsAre;

namespace {
constexpr char kAwsInstanceDynamicDataUrl[] =
    "http://169.254.169.254/latest/dynamic/instance-identity/document";
constexpr char kAuthorizationHeaderKey[] = "X-aws-ec2-metadata-token";
constexpr char kSessionTokenMock[] = "session-token-test";
constexpr char kAwsInstanceResourceNameFormat[] =
    "arn:aws:ec2:%s:%s:instance/%s";
constexpr char kAwsInstanceResourceNameMock[] =
    "arn:aws:ec2:::instance/i-233333";
constexpr char kAwsInstanceResourceNameUsWestMock[] =
    "arn:aws:ec2:us-west-1::instance/i-233333";
constexpr char kBadAwsInstanceResourceNameMock[] =
    "arn:aws:ec2::instance/i-233333";
constexpr char kBadAwsRegionCodeMock[] =
    "arn:aws:ec2:wrong-region-code::instance/i-233333";
constexpr char kRgionUsEast1[] = "us-east-1";
constexpr char kRgionUsWest1[] = "us-west-1";
constexpr char kInstanceIdMock[] = "i-233333";
constexpr char kPrivateIpMock[] = "0.0.0.0";
constexpr char kPublicIpMock[] = "255.255.255.255";

constexpr char kTagName1[] = "/service/tag_name_1";
constexpr char kTagName2[] = "/service/tag_name_2";
constexpr char kTagValue1[] = "tag_value1";
constexpr char kTagValue2[] = "tag_value2";

}  // namespace

namespace google::scp::cpio::client_providers::test {

class MockAwsEC2ClientFactory : public AwsEC2ClientFactory {
 public:
  MOCK_METHOD(core::ExecutionResultOr<std::shared_ptr<Aws::EC2::EC2Client>>,
              CreateClient,
              (const std::string&,
               const std::shared_ptr<core::AsyncExecutorInterface>&),
              (noexcept, override));
};

class AwsInstanceClientProviderTest : public TestWithParam<string> {
 protected:
  AwsInstanceClientProviderTest()
      : http_client_(make_shared<MockCurlClient>()),
        authorizer_provider_(make_shared<MockAuthTokenProvider>()),
        ec2_factory_(make_shared<NiceMock<MockAwsEC2ClientFactory>>()),
        instance_provider_(make_unique<AwsInstanceClientProvider>(
            authorizer_provider_, http_client_,
            make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
            ec2_factory_)) {
    InitAPI(options_);

    ec2_client_ = make_shared<NiceMock<MockEC2Client>>();

    ON_CALL(*ec2_factory_, CreateClient).WillByDefault(Return(ec2_client_));

    EXPECT_SUCCESS(instance_provider_->Init());
    EXPECT_SUCCESS(instance_provider_->Run());
  }

  ~AwsInstanceClientProviderTest() {
    EXPECT_SUCCESS(instance_provider_->Stop());
    ShutdownAPI(options_);
  }

  string GetResponseBody() { return GetParam(); }

  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      fetch_token_context_;

  shared_ptr<MockCurlClient> http_client_;
  shared_ptr<MockAuthTokenProvider> authorizer_provider_;
  shared_ptr<MockEC2Client> ec2_client_;
  shared_ptr<MockAwsEC2ClientFactory> ec2_factory_;
  unique_ptr<AwsInstanceClientProvider> instance_provider_;

  SDKOptions options_;
};

TEST_F(AwsInstanceClientProviderTest, GetCurrentInstanceResourceNameSuccess) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  auto dynamic_data_mock = R"""(
        {
          "accountId" : "123456789",
          "architecture" : "x86_64",
          "availabilityZone" : "us-east-1c",
          "billingProducts" : null,
          "devpayProductCodes" : null,
          "marketplaceProductCodes" : null,
          "imageId" : "ami-01234667899",
          "instanceId" : "i-1234567890",
          "instanceType" : "m5.2xlarge",
          "kernelId" : null,
          "pendingTime" : "2022-08-01T22:27:06Z",
          "privateIp" : "0.0.0.0",
          "ramdiskId" : null,
          "region" : "us-east-1",
          "version" : "2017-09-30"
        }
  )""";

  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(kAwsInstanceDynamicDataUrl)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(
                        Pair(kAuthorizationHeaderKey, kSessionTokenMock))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(dynamic_data_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_EQ(
                context.response->instance_resource_name(),
                absl::StrFormat(kAwsInstanceResourceNameFormat, "us-east-1",
                                "123456789", "i-1234567890"));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetCurrentInstanceResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsInstanceClientProviderTest,
       GetCurrentInstanceResourceNameSyncSuccess) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  auto dynamic_data_mock = R"""(
        {
          "accountId" : "123456789",
          "architecture" : "x86_64",
          "availabilityZone" : "us-east-1c",
          "billingProducts" : null,
          "devpayProductCodes" : null,
          "marketplaceProductCodes" : null,
          "imageId" : "ami-01234667899",
          "instanceId" : "i-1234567890",
          "instanceType" : "m5.2xlarge",
          "kernelId" : null,
          "pendingTime" : "2022-08-01T22:27:06Z",
          "privateIp" : "0.0.0.0",
          "ramdiskId" : null,
          "region" : "us-east-1",
          "version" : "2017-09-30"
        }
  )""";

  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(kAwsInstanceDynamicDataUrl)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(
                        Pair(kAuthorizationHeaderKey, kSessionTokenMock))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(dynamic_data_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  string resource_name;
  EXPECT_THAT(
      instance_provider_->GetCurrentInstanceResourceNameSync(resource_name),
      IsSuccessful());
  EXPECT_EQ(resource_name,
            absl::StrFormat(kAwsInstanceResourceNameFormat, "us-east-1",
                            "123456789", "i-1234567890"));
}

TEST_F(AwsInstanceClientProviderTest,
       GetCurrentInstanceResourceNameSyncFailedWithHttpPerformRequest) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(kAwsInstanceDynamicDataUrl)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(
                        Pair(kAuthorizationHeaderKey, kSessionTokenMock))));
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  string resource_name;
  EXPECT_THAT(
      instance_provider_->GetCurrentInstanceResourceNameSync(resource_name),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  EXPECT_THAT(resource_name, IsEmpty());
}

TEST_F(AwsInstanceClientProviderTest,
       GetCurrentInstanceResourceNameFailedWithMalformedResponse) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });
  // Malformed response missing instanceId field.
  auto dynamic_data_mock = R"""(
        {
          "accountId" : "123456789",
          "architecture" : "x86_64",
          "availabilityZone" : "us-east-1c",
          "billingProducts" : null,
          "devpayProductCodes" : null,
          "marketplaceProductCodes" : null,
          "imageId" : "ami-01234667899",
          "instanceType" : "m5.2xlarge",
          "kernelId" : null,
          "pendingTime" : "2022-08-01T22:27:06Z",
          "privateIp" : "0.0.0.0",
          "ramdiskId" : null,
          "region" : "us-east-1",
          "version" : "2017-09-30"
        }
  )""";

  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(kAwsInstanceDynamicDataUrl)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(
                        Pair(kAuthorizationHeaderKey, kSessionTokenMock))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(dynamic_data_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  auto malformed_failure = FailureExecutionResult(
      SC_AWS_INSTANCE_CLIENT_INSTANCE_RESOURCE_NAME_RESPONSE_MALFORMED);
  atomic<bool> condition{false};
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_THAT(context.result, ResultIs(malformed_failure));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetCurrentInstanceResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsInstanceClientProviderTest,
       GetCurrentInstanceResourceNameWithTokenFetchingFailure) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*http_client_, PerformRequest).Times(0);

  atomic<bool> condition{false};
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetCurrentInstanceResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsInstanceClientProviderTest,
       GetCurrentInstanceResourceNameWithHttpPerformFailure) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*http_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(kAwsInstanceDynamicDataUrl)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(
                        Pair(kAuthorizationHeaderKey, kSessionTokenMock))));
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetCurrentInstanceResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

MATCHER_P(InstanceIdMatches, instance_id, "") {
  return ExplainMatchResult(UnorderedElementsAre(instance_id),
                            arg.GetInstanceIds(), result_listener);
}

TEST_F(AwsInstanceClientProviderTest,
       GetInstanceDetailsByResourceNameSyncSuccess) {
  EXPECT_CALL(*ec2_client_,
              DescribeInstancesAsync(InstanceIdMatches(kInstanceIdMock), _, _))
      .WillOnce([&](auto, auto callback, auto) {
        DescribeInstancesRequest request;
        Instance instance;
        instance.SetInstanceId(kInstanceIdMock);
        instance.SetPrivateIpAddress(kPrivateIpMock);
        instance.SetPublicIpAddress(kPublicIpMock);
        Tag tag_1;
        tag_1.SetKey(kTagName1);
        tag_1.SetValue(kTagValue1);
        instance.AddTags(tag_1);
        Tag tag_2;
        tag_2.SetKey(kTagName2);
        tag_2.SetValue(kTagValue2);
        instance.AddTags(tag_2);
        Reservation reservation;
        reservation.AddInstances(instance);
        DescribeInstancesResponse response;
        response.AddReservations(reservation);
        DescribeInstancesOutcome outcome(move(response));
        callback(nullptr, request, move(outcome), nullptr);
      });

  InstanceDetails details;
  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceNameSync(
                  kAwsInstanceResourceNameMock, details),
              IsSuccessful());
  EXPECT_EQ(details.instance_id(), kInstanceIdMock);
  EXPECT_EQ(details.networks(0).public_ipv4_address(), kPublicIpMock);
  EXPECT_EQ(details.networks(0).private_ipv4_address(), kPrivateIpMock);
  EXPECT_THAT(details.labels(),
              UnorderedElementsAre(Pair(kTagName1, kTagValue1),
                                   Pair(kTagName2, kTagValue2)));
}

TEST_F(AwsInstanceClientProviderTest,
       GetInstanceDetailsByResourceNameSyncFailedWithEC2Client) {
  EXPECT_CALL(*ec2_client_,
              DescribeInstancesAsync(InstanceIdMatches(kInstanceIdMock), _, _))
      .WillOnce([&](auto, auto callback, auto) {
        DescribeInstancesRequest request;
        AWSError<EC2Errors> error(EC2Errors::INTERNAL_FAILURE, false);
        DescribeInstancesOutcome outcome(move(error));
        callback(nullptr, request, move(outcome), nullptr);
      });

  InstanceDetails details;
  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceNameSync(
                  kAwsInstanceResourceNameMock, details),
              ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
}

TEST_F(AwsInstanceClientProviderTest, GetInstanceDetailsByResourceName) {
  EXPECT_CALL(*ec2_client_,
              DescribeInstancesAsync(InstanceIdMatches(kInstanceIdMock), _, _))
      .WillOnce([&](auto, auto callback, auto) {
        DescribeInstancesRequest request;
        Instance instance;
        instance.SetInstanceId(kInstanceIdMock);
        instance.SetPrivateIpAddress(kPrivateIpMock);
        instance.SetPublicIpAddress(kPublicIpMock);
        Reservation reservation;
        reservation.AddInstances(instance);
        DescribeInstancesResponse response;
        response.AddReservations(reservation);
        DescribeInstancesOutcome outcome(move(response));
        callback(nullptr, request, move(outcome), nullptr);
      });

  auto request = make_shared<GetInstanceDetailsByResourceNameRequest>();
  request->set_instance_resource_name(kAwsInstanceResourceNameMock);

  atomic<bool> condition{false};
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(request),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_SUCCESS(context.result);
            const auto& details = context.response->instance_details();
            EXPECT_EQ(details.instance_id(), kInstanceIdMock);
            EXPECT_EQ(details.networks(0).public_ipv4_address(), kPublicIpMock);
            EXPECT_EQ(details.networks(0).private_ipv4_address(),
                      kPrivateIpMock);
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsInstanceClientProviderTest,
       GetInstanceDetailsByResourceNameFailedWithDescribeInstancesAsync) {
  EXPECT_CALL(*ec2_client_,
              DescribeInstancesAsync(InstanceIdMatches(kInstanceIdMock), _, _))
      .WillOnce([&](auto, auto callback, auto) {
        DescribeInstancesRequest request;
        AWSError<EC2Errors> error(EC2Errors::INTERNAL_FAILURE, false);
        DescribeInstancesOutcome outcome(move(error));
        callback(nullptr, request, move(outcome), nullptr);
      });

  auto request = make_shared<GetInstanceDetailsByResourceNameRequest>();
  request->set_instance_resource_name(kAwsInstanceResourceNameMock);

  atomic<bool> condition{false};
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(request),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(
                                            SC_AWS_INTERNAL_SERVICE_ERROR)));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsInstanceClientProviderTest,
       GetInstanceDetailsByResourceNameFailedWithMalformedResponse) {
  EXPECT_CALL(*ec2_client_,
              DescribeInstancesAsync(InstanceIdMatches(kInstanceIdMock), _, _))
      .WillOnce([&](auto, auto callback, auto) {
        DescribeInstancesRequest request;
        Reservation reservation;
        DescribeInstancesResponse response;
        response.AddReservations(reservation);
        DescribeInstancesOutcome outcome(move(response));
        callback(nullptr, request, move(outcome), nullptr);
      });

  auto request = make_shared<GetInstanceDetailsByResourceNameRequest>();
  request->set_instance_resource_name(kAwsInstanceResourceNameMock);

  auto failure = FailureExecutionResult(
      SC_AWS_INSTANCE_CLIENT_PROVIDER_DESCRIBE_INSTANCES_RESPONSE_MALFORMED);
  atomic<bool> condition{false};
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(request),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result, ResultIs(failure));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

MATCHER_P(FilterResourceIdMatches, resource_name, "") {
  return ExplainMatchResult(UnorderedElementsAre(resource_name),
                            arg.GetFilters()[0].GetValues(), result_listener);
}

TEST_F(AwsInstanceClientProviderTest, GetTagsByResourceNameSucceed) {
  EXPECT_CALL(*ec2_client_,
              DescribeTagsAsync(FilterResourceIdMatches(kInstanceIdMock), _, _))
      .WillOnce([&](auto, auto callback, auto) {
        DescribeTagsRequest request;
        DescribeTagsResponse response;
        TagDescription tag_1;
        tag_1.SetKey(kTagName1);
        tag_1.SetValue(kTagValue1);
        response.AddTags(tag_1);
        TagDescription tag_2;
        tag_2.SetKey(kTagName2);
        tag_2.SetValue(kTagValue2);
        response.AddTags(tag_2);
        DescribeTagsOutcome outcome(move(response));
        callback(nullptr, request, move(outcome), nullptr);
      });

  auto request = make_shared<GetTagsByResourceNameRequest>();
  request->set_resource_name(kAwsInstanceResourceNameMock);

  atomic<bool> condition{false};
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(move(request),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                EXPECT_SUCCESS(context.result);
                EXPECT_THAT(context.response->tags(),
                            UnorderedElementsAre(
                                Pair(string(kTagName1), string(kTagValue1)),
                                Pair(string(kTagName2), string(kTagValue2))));
                condition.store(true);
              });

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithDescribeTagsAsync) {
  EXPECT_CALL(*ec2_client_,
              DescribeTagsAsync(FilterResourceIdMatches(kInstanceIdMock), _, _))
      .WillOnce([&](auto, auto callback, auto) {
        DescribeTagsRequest request;
        AWSError<EC2Errors> error(EC2Errors::INTERNAL_FAILURE, false);
        DescribeTagsOutcome outcome(move(error));
        callback(nullptr, request, move(outcome), nullptr);
      });

  auto request = make_shared<GetTagsByResourceNameRequest>();
  request->set_resource_name(kAwsInstanceResourceNameMock);

  atomic<bool> condition{false};
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(move(request),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                EXPECT_THAT(context.result,
                            ResultIs(FailureExecutionResult(
                                SC_AWS_INTERNAL_SERVICE_ERROR)));
                condition.store(true);
              });

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithInvalidResourceName) {
  EXPECT_CALL(*ec2_client_, DescribeTagsAsync).Times(0);

  auto request = make_shared<GetTagsByResourceNameRequest>();
  request->set_resource_name(kBadAwsInstanceResourceNameMock);

  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(move(request),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {});

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              ResultIs(FailureExecutionResult(
                  SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)));
}

TEST_F(AwsInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithInvalidRegionCode) {
  EXPECT_CALL(*ec2_client_, DescribeTagsAsync).Times(0);

  EXPECT_CALL(*ec2_factory_, CreateClient).Times(0);

  auto request = make_shared<GetTagsByResourceNameRequest>();
  request->set_resource_name(kBadAwsRegionCodeMock);

  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(move(request),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {});

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              ResultIs(FailureExecutionResult(
                  SC_AWS_INSTANCE_CLIENT_INVALID_REGION_CODE)));
}

MATCHER_P(RegionMatched, region, "") {
  return ExplainMatchResult(Eq(region), arg, result_listener);
}

TEST_F(AwsInstanceClientProviderTest, GetTagsByResourceNameEC2ClientCached) {
  EXPECT_CALL(*ec2_client_,
              DescribeTagsAsync(FilterResourceIdMatches(kInstanceIdMock), _, _))
      .Times(4)
      .WillRepeatedly([&](auto, auto callback, auto) {
        DescribeTagsRequest request;
        DescribeTagsResponse response;
        DescribeTagsOutcome outcome(move(response));
        callback(nullptr, request, move(outcome), nullptr);
      });

  EXPECT_CALL(*ec2_factory_, CreateClient(RegionMatched(kRgionUsEast1), _))
      .WillOnce(Return(ec2_client_));
  EXPECT_CALL(*ec2_factory_, CreateClient(RegionMatched(kRgionUsWest1), _))
      .WillOnce(Return(ec2_client_));

  atomic<int> condition{0};
  auto request_empty_region = make_shared<GetTagsByResourceNameRequest>();
  request_empty_region->set_resource_name(kAwsInstanceResourceNameMock);
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context_empty_region(
          move(request_empty_region),
          [&](AsyncContext<GetTagsByResourceNameRequest,
                           GetTagsByResourceNameResponse>& context) {
            EXPECT_SUCCESS(context.result);
            condition++;
          });

  auto request_us_west = make_shared<GetTagsByResourceNameRequest>();
  request_us_west->set_resource_name(kAwsInstanceResourceNameUsWestMock);
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context_us_west(
          move(request_us_west),
          [&](AsyncContext<GetTagsByResourceNameRequest,
                           GetTagsByResourceNameResponse>& context) {
            EXPECT_SUCCESS(context.result);
            condition++;
          });

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context_empty_region),
              IsSuccessful());
  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context_empty_region),
              IsSuccessful());
  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context_us_west),
              IsSuccessful());
  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context_us_west),
              IsSuccessful());
  WaitUntil([&]() { return condition.load() == 4; });
}

}  // namespace google::scp::cpio::client_providers::test
