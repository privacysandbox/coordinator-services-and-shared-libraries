// Copyright 2025 Google LLC
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
#include <string>

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "cc/core/common/operation_dispatcher/src/retry_strategy.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/http2_client/src/synchronous_http2_client.h"
#include "cc/core/test/utils/docker_helper/docker_helper.h"
#include "cc/pbs/proto/storage/budget_value.pb.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_connection.h"
#include "google/cloud/status_or.h"
#include "google/protobuf/text_format.h"
#include "proto/pbs/api/v1/api.pb.h"

namespace privacy_sandbox::pbs_common {
namespace {
using ::absl_testing::IsOk;
using ::google::cloud::spanner_admin::DatabaseAdminClient;
using ::google::cloud::spanner_admin::InstanceAdminClient;
using ::google::cloud::spanner_admin::MakeDatabaseAdminConnection;
using ::google::protobuf::TextFormat;
using ::google::protobuf::json::MessageToJsonString;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs_common::Byte;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::GetIpAddress;
using ::privacy_sandbox::pbs_common::HttpClientOptions;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::HttpMethod;
using ::privacy_sandbox::pbs_common::HttpRequest;
using ::privacy_sandbox::pbs_common::kDefaultMaxConnectionsPerHost;
using ::privacy_sandbox::pbs_common::RetryStrategyOptions;
using ::privacy_sandbox::pbs_common::RetryStrategyType;
using ::privacy_sandbox::pbs_common::SyncHttpClient;
using ::privacy_sandbox::pbs_common::TimeDuration;
using ::privacy_sandbox::pbs_common::ToString;
using ::privacy_sandbox::pbs_common::Uuid;
using ::testing::Values;
using ::testing::WithParamInterface;

constexpr absl::string_view kNetworkName = "pbse2etestnetwork";
constexpr absl::string_view kSpannerEmulatorName = "spanner";
constexpr absl::string_view kSpannerGrpcPort = "9010";
constexpr absl::string_view kSpannerProjectId = "my-project";
constexpr absl::string_view kSpannerInstanceName = "myinstance";
constexpr absl::string_view kSpannerDatabaseName = "mydatabase";
constexpr absl::string_view kSpanerBudgetTableName = "budget";
constexpr absl::string_view kSpannerCreateDatabaseStatement =
    "CREATE TABLE budget ("
    "  Budget_Key STRING(MAX) NOT NULL,"
    "  Timeframe STRING(MAX),"
    "  Value JSON"
    ") PRIMARY KEY(Budget_Key, Timeframe)";
constexpr absl::string_view kPbsContainerName = "pbs";
constexpr absl::string_view kPbsHttpPort = "9090";
constexpr absl::string_view kPbsHealthCheckPort = "9091";
constexpr absl::string_view kPbsServerImageLocation =
    "cc/pbs/deploy/pbs_server/build_defs/pbs_cloud_run_container_for_local.tar";
constexpr absl::string_view kDockerComposeLocation =
    "cc/pbs/test/e2e/docker-compose.yaml";
constexpr TimeDuration kHTTPClientBackoffDurationInMs = 10;
constexpr size_t kHTTPClientMaxRetries = 6;
constexpr TimeDuration kHttp2ReadTimeoutInSeconds = 5;

InstanceAdminClient CreateSpannerInstanceAdminClient() {
  auto instance_admin_connection =
      google::cloud::spanner_admin::MakeInstanceAdminConnection();
  return InstanceAdminClient(instance_admin_connection);
}

google::cloud::StatusOr<google::spanner::admin::instance::v1::Instance>
CreateSpannerInstance(InstanceAdminClient& client) {
  google::spanner::admin::instance::v1::Instance spanner_instance;
  spanner_instance.set_node_count(1);
  auto create_instance_future = client.CreateInstance(
      absl::StrCat("projects/", kSpannerProjectId),
      std::string(kSpannerInstanceName), spanner_instance);
  return create_instance_future.get();
}

DatabaseAdminClient CreateDatabaseAdminClient() {
  auto spanner_admin_connection_ = MakeDatabaseAdminConnection();
  return DatabaseAdminClient(spanner_admin_connection_);
}

google::cloud::StatusOr<google::spanner::admin::database::v1::Database>
CreateDatabase(const google::spanner::admin::instance::v1::Instance& instance,
               DatabaseAdminClient& client) {
  auto database_future = client.CreateDatabase(
      instance.name(),
      absl::StrCat("CREATE DATABASE ", std::string(kSpannerDatabaseName)));
  return database_future.get();
}

google::cloud::StatusOr<
    google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
AddProtoTypeColumns(
    const google::spanner::admin::database::v1::Database& database,
    DatabaseAdminClient& client) {
  google::spanner::admin::database::v1::UpdateDatabaseDdlRequest request;
  request.set_database(database.name());
  google::protobuf::FileDescriptorSet fds;
  privacy_sandbox_pbs::BudgetValue::default_instance()
      .GetMetadata()
      .descriptor->file()
      ->CopyTo(fds.add_file());
  fds.SerializeToString(request.mutable_proto_descriptors());
  request.add_statements(std::string(kSpannerCreateDatabaseStatement));
  request.add_statements(
      "CREATE PROTO BUNDLE (privacy_sandbox_pbs.BudgetValue)");
  request.add_statements(absl::StrFormat(
      "ALTER TABLE %s ADD COLUMN ValueProto privacy_sandbox_pbs.BudgetValue",
      kSpanerBudgetTableName));
  return client.UpdateDatabaseDdl(request).get();
}

void SetupSpannerDatabase(absl::string_view ip_address) {
  std::string emulator_http_url =
      absl::StrCat(ip_address, ":", kSpannerGrpcPort);
  setenv("SPANNER_EMULATOR_HOST", emulator_http_url.c_str(), 1);
  InstanceAdminClient instance_admin_client =
      CreateSpannerInstanceAdminClient();
  google::cloud::StatusOr<google::spanner::admin::instance::v1::Instance>
      spanner_instance = CreateSpannerInstance(instance_admin_client);
  ASSERT_TRUE(spanner_instance.status().ok()) << spanner_instance.status();
  DatabaseAdminClient database_admin_client = CreateDatabaseAdminClient();
  google::cloud::StatusOr<google::spanner::admin::database::v1::Database>
      database = CreateDatabase(*spanner_instance, database_admin_client);
  ASSERT_TRUE(database.status().ok()) << database.status();
  AddProtoTypeColumns(*database, database_admin_client);
}

HttpHeaders CreateHttpHeaders() {
  HttpHeaders headers;
  headers.insert({
      {"x-auth-token", "unused"},
      {"x-gscp-claimed-identity", "https://fake.com"},
      {"x-gscp-transaction-id", "00000000-0000-0000-0000-000000000000"},
      {"user-agent", "testing/1.2.3"},
  });
  return headers;
}

absl::StatusOr<std::string> CreateBinaryRequestBodyV2() {
  std::string proto_string = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys {
        # the key field will be added later
        budget_type: BUDGET_TYPE_BINARY_BUDGET
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
    data {
      reporting_origin: "http://b.fake.com"
      keys {
        # the key field will be added later
        budget_type: BUDGET_TYPE_BINARY_BUDGET
        tokens { token_int32: 1 }
        reporting_time: "2019-12-11T07:20:50.52Z"
      }
    }
  )pb";

  ConsumePrivacyBudgetRequest req;
  if (!TextFormat::ParseFromString(proto_string, &req)) {
    return absl::FailedPreconditionError(
        "Couldn't parse proto from the string");
  }

  for (auto& data : *req.mutable_data()) {
    for (auto& key : *data.mutable_keys()) {
      key.set_key(ToString(Uuid::GenerateUuid()));
    }
  }

  std::string json_string;
  if (auto status = MessageToJsonString(req, &json_string); !status.ok()) {
    return status;
  }
  return json_string;
}

class PBSIntegrationTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    ASSERT_EQ(LoadImage(std::string(kPbsServerImageLocation)), 0);

    // Assure that we can run docker compose within our test
    ASSERT_EQ(RunDockerComposeCmd("version"), 0);
    ASSERT_EQ(RunDockerComposeCmd(absl::StrFormat(
                  "--file %s up --wait --detach", kDockerComposeLocation)),
              0);

    std::string emulator_ip_address = GetIpAddress(
        std::string(kNetworkName), std::string(kSpannerEmulatorName));
    SetupSpannerDatabase(emulator_ip_address);
  }

  void SetUp() override {
    HttpClientOptions http_client_options(
        RetryStrategyOptions(RetryStrategyType::Linear,
                             kHTTPClientBackoffDurationInMs,
                             kHTTPClientMaxRetries),
        kDefaultMaxConnectionsPerHost, kHttp2ReadTimeoutInSeconds);
    http_client_ = std::make_shared<SyncHttpClient>(http_client_options);
    pbs_url_ = absl::StrCat(
        "http://",
        GetIpAddress(std::string(kNetworkName), std::string(kPbsContainerName)),
        ":", kPbsHttpPort);
    pbs_health_check_url_ = absl::StrCat(
        "http://",
        GetIpAddress(std::string(kNetworkName), std::string(kPbsContainerName)),
        ":", kPbsHealthCheckPort);
  }

  static void TearDownTestSuite() {
    ASSERT_EQ(RunDockerComposeCmd(
                  absl::StrFormat("--file %s  down", kDockerComposeLocation)),
              0);
  }

  ExecutionResult PerformRequest(absl::string_view path,
                                 absl::string_view request_body,
                                 const HttpHeaders& headers) {
    HttpRequest http_request;
    http_request.path =
        std::make_shared<std::string>(absl::StrCat(pbs_url_, path));
    http_request.method = HttpMethod::POST;
    http_request.body.length = request_body.size();
    http_request.body.bytes = std::make_shared<std::vector<Byte>>(
        request_body.begin(), request_body.end());
    http_request.headers = std::make_shared<HttpHeaders>(headers);

    return http_client_->PerformRequest(http_request).execution_result;
  }

  std::shared_ptr<SyncHttpClient> http_client_;
  std::string pbs_url_;
  std::string pbs_health_check_url_;
};

class PBSIntegrationTestWithBudgetType
    : public PBSIntegrationTest,
      public WithParamInterface<
          ConsumePrivacyBudgetRequest::PrivacyBudgetKey::BudgetType> {
 protected:
  void SetUp() override { PBSIntegrationTest::SetUp(); }

  void TearDown() override { PBSIntegrationTest::TearDown(); }

  absl::StatusOr<std::string> CreateRequestBodyV2() {
    switch (GetParam()) {
      case ConsumePrivacyBudgetRequest::PrivacyBudgetKey::
          BUDGET_TYPE_BINARY_BUDGET:
        return CreateBinaryRequestBodyV2();
      default:
        return absl::FailedPreconditionError("Unsupported budget type");
    }
  }
};

INSTANTIATE_TEST_SUITE_P(PBSIntegrationTestWithBudgetType,
                         PBSIntegrationTestWithBudgetType,
                         Values(ConsumePrivacyBudgetRequest::PrivacyBudgetKey::
                                    BUDGET_TYPE_BINARY_BUDGET));

TEST_F(PBSIntegrationTest, HealthCheck) {
  HttpRequest http_request{};
  http_request.path = std::make_shared<std::string>(
      absl::StrCat(pbs_health_check_url_, "/health"));
  http_request.method = HttpMethod::GET;
  http_request.body.length = 0;
  http_request.headers = std::make_shared<HttpHeaders>();

  EXPECT_SUCCESS(http_client_->PerformRequest(http_request).execution_result);
}

TEST_P(PBSIntegrationTestWithBudgetType, ConsumeBudgetV2FivePhases) {
  HttpHeaders headers = CreateHttpHeaders();
  absl::StatusOr<std::string> request_body = CreateRequestBodyV2();
  ASSERT_THAT(request_body, IsOk());
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:begin", *request_body, headers));
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:prepare", *request_body, headers));
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:commit", *request_body, headers));
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:notify", *request_body, headers));
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:end", *request_body, headers));
}

TEST_P(PBSIntegrationTestWithBudgetType, ConsumeBudgetV2TwoPhases) {
  HttpHeaders headers = CreateHttpHeaders();
  absl::StatusOr<std::string> request_body = CreateRequestBodyV2();
  ASSERT_THAT(request_body, IsOk());
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:health-check", *request_body, headers));
  EXPECT_SUCCESS(PerformRequest("/v1/transactions:consume-budget",
                                *request_body, headers));
}
}  // namespace
}  // namespace privacy_sandbox::pbs_common
