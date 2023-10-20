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

#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/instance_client_provider/src/aws/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using absl::StrFormat;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using google::scp::cpio::client_providers::AwsResourceNameDetails;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::atomic;
using std::get;
using std::make_shared;
using std::make_tuple;
using std::move;
using std::shared_ptr;
using std::string;
using std::tuple;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::TestWithParam;
using testing::UnorderedElementsAre;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";

constexpr char kResourceNameWithPathMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/Dev/i-0e9801d129EXAMPLE";
constexpr char kResourceNameNoRegionMock[] =
    "arn:aws:ec2::123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kResourceNameNoAccountMock[] =
    "arn:aws:ec2:us-east-1::instance/i-0e9801d129EXAMPLE";

constexpr char kInstanceIdMock[] = "i-0e9801d129EXAMPLE";
constexpr char kRegionMock[] = "us-east-1";
constexpr char kAccountIdMock[] = "123456789012";
}  // namespace

namespace google::scp::cpio::client_providers::test {

TEST(AwsInstanceClientUtilsTest, GetCurrentRegionCodeSuccess) {
  auto instance_client = make_shared<MockInstanceClientProvider>();
  instance_client->instance_resource_name = kResourceNameMock;

  auto region_code =
      AwsInstanceClientUtils::GetCurrentRegionCode(instance_client);
  EXPECT_EQ(*region_code, kRegionMock);
}

TEST(AwsInstanceClientUtilsTest, GetCurrentRegionCodeFailedWithResourceName) {
  auto instance_client = make_shared<MockInstanceClientProvider>();
  instance_client->get_instance_resource_name_mock =
      FailureExecutionResult(SC_UNKNOWN);

  auto region_code =
      AwsInstanceClientUtils::GetCurrentRegionCode(instance_client);
  EXPECT_THAT(region_code.result(),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST(AwsInstanceClientUtilsTest, ValidateResourceNameFormat) {
  EXPECT_THAT(
      AwsInstanceClientUtils::ValidateResourceNameFormat(kResourceNameMock),
      IsSuccessful());
}

TEST(AwsInstanceClientUtilsTest, ParseInstanceIdFromInstanceResourceName) {
  EXPECT_THAT(AwsInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
                  kResourceNameMock),
              IsSuccessfulAndHolds(kInstanceIdMock));
  EXPECT_THAT(AwsInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
                  kResourceNameWithPathMock),
              IsSuccessfulAndHolds(kInstanceIdMock));
}

TEST(AwsInstanceClientUtilsTest, ParseRegionFromResourceName) {
  EXPECT_THAT(
      AwsInstanceClientUtils::ParseRegionFromResourceName(kResourceNameMock),
      IsSuccessfulAndHolds(kRegionMock));
  EXPECT_THAT(AwsInstanceClientUtils::ParseRegionFromResourceName(
                  kResourceNameNoRegionMock),
              IsSuccessfulAndHolds(""));
}

TEST(AwsInstanceClientUtilsTest, ParseAccountIdFromResourceName) {
  EXPECT_THAT(
      AwsInstanceClientUtils::ParseAccountIdFromResourceName(kResourceNameMock),
      IsSuccessfulAndHolds(kAccountIdMock));
  EXPECT_THAT(AwsInstanceClientUtils::ParseAccountIdFromResourceName(
                  kResourceNameNoAccountMock),
              IsSuccessfulAndHolds(""));
}

TEST(AwsInstanceClientUtilsTest, GetInstanceResourceNameDetails) {
  AwsResourceNameDetails details;
  EXPECT_THAT(AwsInstanceClientUtils::GetResourceNameDetails(kResourceNameMock,
                                                             details),
              IsSuccessful());
  EXPECT_EQ(details.account_id, kAccountIdMock);
  EXPECT_EQ(details.region, kRegionMock);
  EXPECT_EQ(details.resource_id, kInstanceIdMock);
}

class AwsInstanceClientUtilsTestII
    : public TestWithParam<tuple<std::string, ExecutionResult>> {
 protected:
  string GetResourceName() { return get<0>(GetParam()); }

  ExecutionResult GetExecutionResult() { return get<1>(GetParam()); }
};

TEST_P(AwsInstanceClientUtilsTestII, FailedWithBadResourceName) {
  auto resource_name = GetResourceName();
  EXPECT_THAT(AwsInstanceClientUtils::ValidateResourceNameFormat(resource_name),
              ResultIs(GetExecutionResult()));
}

INSTANTIATE_TEST_SUITE_P(
    ValidateResourceNameFormat, AwsInstanceClientUtilsTestII,
    testing::Values(
        make_tuple(
            "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE",
            SuccessExecutionResult()),
        make_tuple("arn:aws-cn:ec2:us-east-1:123456789012:instance/"
                   "i-0e9801d129EXAMPLE",
                   SuccessExecutionResult()),
        make_tuple("arn:aws:ec2::123456789012:instance/i-0e9801d129EXAMPLE",
                   SuccessExecutionResult()),
        make_tuple("arn:aws:ec2:us-east-1::instance/i-0e9801d129EXAMPLE",
                   SuccessExecutionResult()),
        make_tuple(
            "arn:aws::us-east-1:123456789012:instance/i-0e9801d129EXAMPLE",
            FailureExecutionResult(
                SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)),
        make_tuple("arn:aws:ec2:us-east-1:abc123456789012:instance/"
                   "i-0e9801d129EXAMPLE",
                   FailureExecutionResult(
                       SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)),
        make_tuple(
            "arn:aws-mess:ec2:us-east-1:123456789012:instance/"
            "i-0e9801d129EXAMPLE",
            FailureExecutionResult(
                SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME))));

}  // namespace google::scp::cpio::client_providers::test
