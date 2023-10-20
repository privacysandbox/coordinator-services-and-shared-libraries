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

#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using absl::StrFormat;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::client_providers::GcpInstanceResourceNameDetails;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using std::atomic;
using std::get;
using std::make_shared;
using std::make_tuple;
using std::move;
using std::shared_ptr;
using std::string;
using std::tuple;
using testing::_;
using testing::DoAll;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::TestWithParam;
using testing::UnorderedElementsAre;

namespace {
constexpr char kResourceNameMock[] =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";

constexpr char kInstanceIdMock[] = "987654321";
constexpr char kZoneIdMock[] = "us-central1-c";
constexpr char kProjectIdMock[] = "123456789";

constexpr char kResourceManagerUriFormat[] =
    "https://%scloudresourcemanager.googleapis.com/v3/tagBindings";
}  // namespace

namespace google::scp::cpio::client_providers::test {
TEST(GcpInstanceClientUtilsTest, GetCurrentProjectIdSuccess) {
  auto instance_client = make_shared<MockInstanceClientProvider>();
  instance_client->instance_resource_name = kResourceNameMock;

  auto project_id =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client);
  EXPECT_EQ(*project_id, kProjectIdMock);
}

TEST(GcpInstanceClientUtilsTest, GetCurrentProjectIdFailedWithResourceName) {
  auto instance_client = make_shared<MockInstanceClientProvider>();
  instance_client->get_instance_resource_name_mock =
      FailureExecutionResult(SC_UNKNOWN);

  auto project_id =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client);
  EXPECT_THAT(project_id.result(),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST(GcpInstanceClientUtilsTest, ValidateInstanceResourceNameFormat) {
  EXPECT_THAT(GcpInstanceClientUtils::ValidateInstanceResourceNameFormat(
                  kResourceNameMock),
              IsSuccessful());
}

TEST(GcpInstanceClientUtilsTest, ParseInstanceIdFromInstanceResourceName) {
  auto instance_id =
      GcpInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
          kResourceNameMock);
  EXPECT_EQ(*instance_id, kInstanceIdMock);
}

TEST(GcpInstanceClientUtilsTest, ParseZoneIdFromInstanceResourceName) {
  auto zone_id = GcpInstanceClientUtils::ParseZoneIdFromInstanceResourceName(
      kResourceNameMock);
  EXPECT_EQ(*zone_id, kZoneIdMock);
}

TEST(GcpInstanceClientUtilsTest, ParseProjectIdFromInstanceResourceName) {
  auto project_id =
      GcpInstanceClientUtils::ParseProjectIdFromInstanceResourceName(
          kResourceNameMock);
  EXPECT_EQ(*project_id, kProjectIdMock);
}

TEST(GcpInstanceClientUtilsTest, GetInstanceResourceNameDetails) {
  GcpInstanceResourceNameDetails details;
  EXPECT_THAT(GcpInstanceClientUtils::GetInstanceResourceNameDetails(
                  kResourceNameMock, details),
              IsSuccessful());
  EXPECT_EQ(details.project_id, kProjectIdMock);
  EXPECT_EQ(details.zone_id, kZoneIdMock);
  EXPECT_EQ(details.instance_id, kInstanceIdMock);
}

class GcpInstanceClientUtilsTestII : public TestWithParam<std::string> {
 protected:
  string GetInstanceResourceName() { return GetParam(); }
};

TEST_P(GcpInstanceClientUtilsTestII, FailedWithBadInstanceResourceName) {
  auto resource_name = GetInstanceResourceName();
  EXPECT_THAT(
      GcpInstanceClientUtils::ValidateInstanceResourceNameFormat(resource_name),
      ResultIs(FailureExecutionResult(
          SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)));
}

INSTANTIATE_TEST_SUITE_P(
    ValidateInstanceResourceNameFormat, GcpInstanceClientUtilsTestII,
    testing::Values(
        R"("projects/123456/zones/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones/us-west1instances/123")",
        R"("//compute.googleapis.com/projects/-123456/zones/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/a123456/zones/12us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones/us-west1/instances/abc")",
        R"("//compute.googleapis.com/projects//zones/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones//instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones/us-west1/instances")",
        R"("//compute.googleapis.com/12345/zones/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones/us-west1/12345")"));

class GcpInstanceClientUtilsTestIII
    : public TestWithParam<tuple<string, string>> {
 protected:
  string GetResourceName() { return get<0>(GetParam()); }

  string GetResourceLocation() { return get<1>(GetParam()); }
};

TEST_P(GcpInstanceClientUtilsTestIII, CreateRMListTagsUrl) {
  auto resource_name = GetResourceName();

  auto path = StrFormat(kResourceManagerUriFormat, GetResourceLocation());
  EXPECT_EQ(GcpInstanceClientUtils::CreateRMListTagsUrl(resource_name), path);
}

INSTANTIATE_TEST_SUITE_P(
    RMListTagsUrl, GcpInstanceClientUtilsTestIII,
    testing::Values(
        make_tuple(
            R"("//run.googleapis.com/projects/PROJECT_ID/locations/LOCATION_ID/services/"
                "SERVICE_ID")",
            "LOCATION_ID-"),
        make_tuple(
            R"("//compute.googleapis.com/projects/PROJECT_ID/zones/ZONE/instances/"
                "INSTANCE_ID")",
            "ZONE-"),
        make_tuple(
            R"("//compute.googleapis.com/projects/PROJECT_ID/regions/REGION/subnetworks/"
                "SUBNETWORK")",
            "REGION-"),
        make_tuple(
            R"("//iam.googleapis.com/projects/PROJECT_ID/serviceAccounts/"
                "SERVICE_ACCOUNT_EMAIL")",
            "")));

}  // namespace google::scp::cpio::client_providers::test
