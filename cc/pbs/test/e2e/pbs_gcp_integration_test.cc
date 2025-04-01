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

#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/common/operation_dispatcher/src/retry_strategy.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/http2_client/src/http2_client.h"
#include "cc/core/test/utils/docker_helper/docker_helper.h"
#include "cc/pbs/proto/storage/budget_value.pb.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_connection.h"
#include "google/cloud/status_or.h"

namespace google::scp::pbs {
namespace {

using ::google::cloud::spanner_admin::DatabaseAdminClient;
using ::google::cloud::spanner_admin::InstanceAdminClient;
using ::google::cloud::spanner_admin::MakeDatabaseAdminConnection;
using ::google::scp::core::common::RetryStrategyOptions;
using ::google::scp::core::common::RetryStrategyType;
using ::google::scp::core::test::CreateNetwork;
using ::google::scp::core::test::GetIpAddress;
using ::google::scp::core::test::LoadImage;
using ::google::scp::core::test::RemoveNetwork;
using ::google::scp::core::test::StartContainer;
using ::google::scp::core::test::StopContainer;
using ::privacy_sandbox::pbs_common::AsyncContext;
using ::privacy_sandbox::pbs_common::AsyncExecutor;
using ::privacy_sandbox::pbs_common::AsyncExecutorInterface;
using ::privacy_sandbox::pbs_common::Byte;
using ::privacy_sandbox::pbs_common::HttpClient;
using ::privacy_sandbox::pbs_common::HttpClientOptions;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::HttpMethod;
using ::privacy_sandbox::pbs_common::HttpRequest;
using ::privacy_sandbox::pbs_common::HttpResponse;
using ::privacy_sandbox::pbs_common::kDefaultMaxConnectionsPerHost;
using ::privacy_sandbox::pbs_common::TimeDuration;

constexpr absl::string_view kNetworkName = "testnetwork";
constexpr absl::string_view kSpannerEmulatorName = "spanner";
constexpr absl::string_view kSpannerGrpcPort = "9010";
constexpr absl::string_view kSpannerGrpcDockerPort = "9010:9010";
constexpr absl::string_view kSpannerHttpDockerPort = "9020:9020";
constexpr absl::string_view kSpannerImageName =
    "gcr.io/cloud-spanner-emulator/emulator:1.5.24";

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
constexpr absl::string_view kPbsHttpDockerPort = "9090:9090";
constexpr absl::string_view kPbsHealthCheckPort = "9091";
constexpr absl::string_view kPbsHealthCheckDockerPort = "9091:9091";
constexpr absl::string_view kPbsServerImageLocation =
    "cc/pbs/deploy/pbs_server/build_defs/pbs_cloud_run_container_for_local.tar";
constexpr absl::string_view kPbsServerImageName =
    "bazel/cc/pbs/deploy/pbs_server/build_defs:pbs_cloud_run_container_local";

constexpr absl::string_view kRequestBodyV2Template = R"(
  {
    "v" : "2.0",
    "data": [
      {
        "reporting_origin": "http://a.fake.com",
        "keys": [
          {
            "key": "%s",
            "token": 1,
            "reporting_time": "2019-12-11T07:20:50.52Z"
          }
        ]
      },
      {
        "reporting_origin": "http://b.fake.com",
        "keys": [
          {
            "key": "%s",
            "token": 1,
            "reporting_time": "2019-12-12T07:20:50.52Z"
          }
        ]
      }
    ]
  })";

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

absl::Status WaitForRunning(absl::string_view network_name,
                            absl::string_view container_name) {
  std::string pbs_ip_address = "";
  for (int i = 0; i < 3 && pbs_ip_address == ""; ++i) {
    // We need to sleep here because that is the only way to figure out whether
    // a container has been running successfully.
    absl::SleepFor(absl::Seconds(3));
    pbs_ip_address =
        GetIpAddress(std::string(network_name), std::string(container_name));
  }
  return pbs_ip_address == "" ? absl::FailedPreconditionError(absl::StrCat(
                                    container_name, " is not running."))
                              : absl::OkStatus();
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

std::string CreateRequestBodyV2() {
  return absl::StrFormat(
      kRequestBodyV2Template,
      core::common::ToString(core::common::Uuid::GenerateUuid()),
      core::common::ToString(core::common::Uuid::GenerateUuid()));
}

class PBSIntegrationTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    // Create the docker network.
    int return_status = CreateNetwork(std::string(kNetworkName));
    ASSERT_EQ(return_status, 0);

    // Start Spanner emulator
    return_status = StartContainer(
        std::string(kNetworkName), std::string(kSpannerEmulatorName),
        std::string(kSpannerImageName), std::string(kSpannerGrpcDockerPort),
        std::string(kSpannerHttpDockerPort), {});
    ASSERT_EQ(return_status, 0);
    ASSERT_TRUE(WaitForRunning(kNetworkName, kSpannerEmulatorName).ok());

    std::string emulator_ip_address = GetIpAddress(
        std::string(kNetworkName), std::string(kSpannerEmulatorName));
    SetupSpannerDatabase(emulator_ip_address);

    return_status = LoadImage(std::string(kPbsServerImageLocation));
    ASSERT_EQ(return_status, 0);

    std::map<std::string, std::string> environment_variables = {
        {"google_scp_otel_enabled", "true"},
        {"google_scp_pbs_host_address", "0.0.0.0"},
        {"google_scp_pbs_host_port", std::string(kPbsHttpPort)},
        {"google_scp_pbs_health_port", std::string(kPbsHealthCheckPort)},
        {"google_scp_pbs_http2_server_use_tls", "false"},
        {"google_scp_pbs_io_async_executor_queue_size", "100"},
        {"google_scp_pbs_io_async_executor_threads_count", "2"},
        {"google_scp_pbs_async_executor_queue_size", "100"},
        {"google_scp_pbs_async_executor_threads_count", "2"},
        {"google_scp_core_http2server_threads_count", "2"},
        {"google_scp_pbs_relaxed_consistency_enabled", "true"},
        {"google_scp_pbs_log_provider", "StdoutLogProvider"},
        {"google_scp_gcp_project_id", std::string(kSpannerProjectId)},
        {"google_scp_spanner_instance_name", std::string(kSpannerInstanceName)},
        {"google_scp_spanner_database_name", std::string(kSpannerDatabaseName)},
        {"google_scp_pbs_budget_key_table_name",
         std::string(kSpanerBudgetTableName)},
        {"google_scp_core_spanner_endpoint_override",
         absl::StrCat(emulator_ip_address, ":", kSpannerGrpcPort)},
        {"SPANNER_EMULATOR_HOST",
         absl::StrCat(emulator_ip_address, ":", kSpannerGrpcPort)},
        // Unused, but required environment variables
        {"google_scp_pbs_transaction_manager_capacity", "-1"},
        {"google_scp_pbs_journal_service_bucket_name", "unused"},
        {"google_scp_pbs_journal_service_partition_name", "unused"},
        {"google_scp_pbs_partition_lock_table_name", "unused"},
        {"google_scp_pbs_partition_lease_duration_in_seconds", "-1"},
        {"google_scp_pbs_remote_claimed_identity", "unused"},
        {"google_scp_pbs_value_proto_migration_phase", "phase_2"},
        {"google_scp_pbs_migration_enable_budget_consumer_migration", "true"},
    };

    return_status = StartContainer(
        std::string(kNetworkName), std::string(kPbsContainerName),
        std::string(kPbsServerImageName), std::string(kPbsHttpDockerPort),
        std::string(kPbsHealthCheckDockerPort), environment_variables);
    ASSERT_EQ(return_status, 0);

    ASSERT_TRUE(WaitForRunning(kNetworkName, kPbsContainerName).ok());
  }

  void SetUp() override {
    async_executor_ = std::make_shared<AsyncExecutor>(
        /*thread_count=*/2, /*queue_cap=*/100,
        /*drop_tasks_on_stop=*/true);
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());

    HttpClientOptions http_client_options(
        RetryStrategyOptions(RetryStrategyType::Linear,
                             kHTTPClientBackoffDurationInMs,
                             kHTTPClientMaxRetries),
        kDefaultMaxConnectionsPerHost, kHttp2ReadTimeoutInSeconds);
    http_client_ =
        std::make_shared<HttpClient>(async_executor_, http_client_options);
    EXPECT_SUCCESS(http_client_->Init());
    EXPECT_SUCCESS(http_client_->Run());
    pbs_url_ = absl::StrCat(
        "http://",
        GetIpAddress(std::string(kNetworkName), std::string(kPbsContainerName)),
        ":", kPbsHttpPort);

    pbs_health_check_url_ = absl::StrCat(
        "http://",
        GetIpAddress(std::string(kNetworkName), std::string(kPbsContainerName)),
        ":", kPbsHealthCheckPort);
  }

  void TearDown() override {
    EXPECT_SUCCESS(http_client_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
  }

  static void TearDownTestSuite() {
    StopContainer(std::string(kPbsContainerName));
    StopContainer(std::string(kSpannerEmulatorName));
    RemoveNetwork(std::string(kNetworkName));
  }

  AsyncContext<HttpRequest, HttpResponse> PerformRequest(
      absl::string_view path, absl::string_view request_body,
      const HttpHeaders& headers) {
    AsyncContext<HttpRequest, HttpResponse> context;
    context.request = std::make_shared<HttpRequest>();

    context.request->path =
        std::make_shared<std::string>(absl::StrCat(pbs_url_, path));
    context.request->method = HttpMethod::POST;
    context.request->body.length = request_body.size();
    context.request->body.bytes = std::make_shared<std::vector<Byte>>(
        request_body.begin(), request_body.end());
    context.request->headers = std::make_shared<HttpHeaders>(headers);

    absl::Notification notification;
    AsyncContext<HttpRequest, HttpResponse> context_result;
    context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
      context_result = context;
      notification.Notify();
    };

    http_client_->PerformRequest(context);
    notification.WaitForNotification();
    return context_result;
  }

  std::shared_ptr<AsyncExecutorInterface> async_executor_;
  std::shared_ptr<HttpClient> http_client_;
  std::string pbs_url_;
  std::string pbs_health_check_url_;
};

TEST_F(PBSIntegrationTest, HealthCheck) {
  AsyncContext<HttpRequest, HttpResponse> context;
  context.request = std::make_shared<HttpRequest>();

  context.request->path = std::make_shared<std::string>(
      absl::StrCat(pbs_health_check_url_, "/health"));
  context.request->method = HttpMethod::GET;
  context.request->body.length = 0;
  context.request->headers = std::make_shared<HttpHeaders>();

  absl::Notification notification;
  context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    EXPECT_SUCCESS(context.result);
    notification.Notify();
  };

  http_client_->PerformRequest(context);
  notification.WaitForNotification();
}

TEST_F(PBSIntegrationTest, ConsumeBudgetV1) {
  std::string request_body = R"(
  {
    "v" : "1.0",
    "t" : [
      {
        "key" : "my-key",
        "token" : 1,
        "reporting_time" : "2021-10-12T07:20:50.52Z"
      },
      {
        "key" : "my-key",
        "token" : 1,
        "reporting_time" : "2019-10-12T07:20:50.52Z"
      }
    ]
  })";

  HttpHeaders headers = CreateHttpHeaders();
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:begin", request_body, headers).result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:prepare", request_body, headers).result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:commit", request_body, headers).result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:notify", request_body, headers).result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:end", request_body, headers).result);
}

TEST_F(PBSIntegrationTest, ConsumeBudgetV2FivePhases) {
  HttpHeaders headers = CreateHttpHeaders();

  std::string request_body = CreateRequestBodyV2();
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:begin", request_body, headers).result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:prepare", request_body, headers).result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:commit", request_body, headers).result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:notify", request_body, headers).result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:end", request_body, headers).result);
}

TEST_F(PBSIntegrationTest, ConsumeBudgetV2TwoPhases) {
  HttpHeaders headers = CreateHttpHeaders();
  std::string request_body = CreateRequestBodyV2();
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:health-check", request_body, headers)
          .result);
  EXPECT_SUCCESS(
      PerformRequest("/v1/transactions:consume-budget", request_body, headers)
          .result);
}

}  // namespace
}  // namespace google::scp::pbs
