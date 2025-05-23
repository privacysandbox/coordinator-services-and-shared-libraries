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

#include "cc/pbs/health_service/src/health_service.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/http2_server/mock/mock_http2_server.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/telemetry/mock/in_memory_metric_router.h"
#include "cc/core/telemetry/src/common/metric_utils.h"
#include "cc/pbs/health_service/src/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"

using ::privacy_sandbox::pbs_common::AsyncContext;
using ::privacy_sandbox::pbs_common::AsyncExecutor;
using ::privacy_sandbox::pbs_common::AsyncExecutorInterface;
using ::privacy_sandbox::pbs_common::ConfigKey;
using ::privacy_sandbox::pbs_common::ConfigProviderInterface;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::ExecutionResultOr;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::GetMetricPointData;
using ::privacy_sandbox::pbs_common::HttpRequest;
using ::privacy_sandbox::pbs_common::HttpResponse;
using ::privacy_sandbox::pbs_common::HttpServerInterface;
using ::privacy_sandbox::pbs_common::InMemoryMetricRouter;
using ::privacy_sandbox::pbs_common::IsSuccessful;
using ::privacy_sandbox::pbs_common::MockHttp2Server;
using ::privacy_sandbox::pbs_common::ResultIs;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::filesystem::space_info;
using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgReferee;

namespace privacy_sandbox::pbs {

class ConfigProviderMock : public ConfigProviderInterface {
 public:
  ConfigProviderMock() {}

  ~ConfigProviderMock() {}

  MOCK_METHOD(ExecutionResult, Get, ((const ConfigKey& key), (bool& out)),
              (noexcept, override));

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Get(const ConfigKey& key, size_t& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key, int32_t& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key, string& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      list<string>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      list<int32_t>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      list<size_t>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key, list<bool>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key, double& out) noexcept override {
    return SuccessExecutionResult();
  }
};

class HealthServiceForTests : public HealthService {
 public:
  bool mem_and_storage_health_was_checked = false;

  HealthServiceForTests() : HealthService() {}

  HealthServiceForTests(shared_ptr<HttpServerInterface>& http_server,
                        shared_ptr<ConfigProviderInterface>& config_provider,
                        shared_ptr<AsyncExecutorInterface>& async_executor)
      : HealthService(http_server, config_provider, async_executor) {}

  void SetMemInfoFilePath(const string& meminfo_file_path) {
    meminfo_file_path_ = meminfo_file_path;
  }

  string GetMemInfoFilePath() noexcept override { return meminfo_file_path_; }

  void SetFileSystemSpaceInfo(
      const ExecutionResultOr<space_info>& fs_space_info) {
    fs_space_info_ = fs_space_info;
  }

  ExecutionResultOr<space_info> GetFileSystemSpaceInfo(
      string directory) noexcept override {
    return fs_space_info_;
  }

  ExecutionResult CheckHealth(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    return HealthService::CheckHealth(http_context);
  }

  ExecutionResult CheckMemoryAndStorageUsage() noexcept override {
    mem_and_storage_health_was_checked = true;
    return HealthService::CheckMemoryAndStorageUsage();
  }

  ExecutionResultOr<int> GetMemoryUsagePercentage() {
    return HealthService::GetMemoryUsagePercentage();
  }

  ExecutionResultOr<int> GetFileSystemStorageUsagePercentage(
      const string& directory) {
    return HealthService::GetFileSystemStorageUsagePercentage(directory);
  }

 private:
  string meminfo_file_path_;
  ExecutionResultOr<space_info> fs_space_info_;
};

class HealthServiceTest : public ::testing::Test {
 protected:
  HealthServiceTest() {
    config_provider_mock_ = make_shared<NiceMock<ConfigProviderMock>>();
    http_server_ = make_shared<MockHttp2Server>();
    async_executor_ =
        make_shared<AsyncExecutor>(2 /* thread count */, 10000 /* queue cap */);
    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());

    // Initialize OTel for testing
    metric_router_ = std::make_unique<InMemoryMetricRouter>();

    // Make memory and storage checking enabled by default
    ON_CALL(*dynamic_cast<ConfigProviderMock*>(config_provider_mock_.get()),
            Get(kPBSHealthServiceEnableMemoryAndStorageCheck, _))
        .WillByDefault(
            DoAll(SetArgReferee<1>(true), Return(SuccessExecutionResult())));

    health_service_ = HealthServiceForTests(http_server_, config_provider_mock_,
                                            async_executor_);

    // Always be good on memory and drive usage
    health_service_.SetMemInfoFilePath(
        "cc/pbs/health_service/test/files/five_percent_meminfo_file.txt");
    space_info fs_space_info;
    fs_space_info.capacity = 100;
    fs_space_info.available = 80;
    health_service_.SetFileSystemSpaceInfo(fs_space_info);

    EXPECT_SUCCESS(health_service_.Init());
  }

  ~HealthServiceTest() { EXPECT_SUCCESS(async_executor_->Stop()); }

  HealthServiceForTests health_service_;
  shared_ptr<HttpServerInterface> http_server_;
  shared_ptr<ConfigProviderInterface> config_provider_mock_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
  // For testing OTel metrics
  std::unique_ptr<InMemoryMetricRouter> metric_router_;
};

TEST_F(HealthServiceTest,
       ShouldReturnHealthyWhenMemoryAndStorageUsageAreBelowThreshold) {
  AsyncContext<HttpRequest, HttpResponse> context;
  auto result = health_service_.CheckHealth(context);
  EXPECT_TRUE(health_service_.mem_and_storage_health_was_checked);
  EXPECT_SUCCESS(result);
  EXPECT_SUCCESS(context.result);
}

TEST_F(HealthServiceTest, ShouldNotCheckMemOrStorageIfCheckingDisabled) {
  // Return false for mem and storage checking
  EXPECT_CALL(*dynamic_cast<ConfigProviderMock*>(config_provider_mock_.get()),
              Get(kPBSHealthServiceEnableMemoryAndStorageCheck, _))
      .WillOnce(
          DoAll(SetArgReferee<1>(false), Return(SuccessExecutionResult())));

  AsyncContext<HttpRequest, HttpResponse> context;
  auto result = health_service_.CheckHealth(context);

  EXPECT_FALSE(health_service_.mem_and_storage_health_was_checked);
  EXPECT_SUCCESS(result);
  EXPECT_SUCCESS(context.result);
}

TEST_F(HealthServiceTest, ShouldNotCheckMemOrStorageIfConfigDoesNotExist) {
  // Failure execution result when reading the config key
  EXPECT_CALL(*dynamic_cast<ConfigProviderMock*>(config_provider_mock_.get()),
              Get(kPBSHealthServiceEnableMemoryAndStorageCheck, _))
      .WillOnce(Return(FailureExecutionResult(SC_UNKNOWN)));

  AsyncContext<HttpRequest, HttpResponse> context;
  auto result = health_service_.CheckHealth(context);

  EXPECT_FALSE(health_service_.mem_and_storage_health_was_checked);
  EXPECT_SUCCESS(result);
  EXPECT_SUCCESS(context.result);
}

TEST_F(HealthServiceTest, ShouldParseMemInfoFileWhenInfoIsAvailable) {
  // Set the path to the file to read meminfo from
  health_service_.SetMemInfoFilePath(
      "cc/pbs/health_service/test/files/five_percent_meminfo_file.txt");

  auto mem_usage_percentage = health_service_.GetMemoryUsagePercentage();
  EXPECT_SUCCESS(mem_usage_percentage);
  EXPECT_EQ(*mem_usage_percentage, 5);

  // Set the path to the file to read meminfo from
  health_service_.SetMemInfoFilePath(
      "cc/pbs/health_service/test/files/ninety_six_percent_meminfo_file.txt");

  mem_usage_percentage = health_service_.GetMemoryUsagePercentage();
  EXPECT_SUCCESS(mem_usage_percentage);
  EXPECT_EQ(*mem_usage_percentage, 96);
}

TEST_F(HealthServiceTest, ShouldIfMemInfoFileIsNotFound) {
  // Set the path to the file to read meminfo from
  health_service_.SetMemInfoFilePath("file/that/does/not/exist.txt");

  auto mem_usage_percentage = health_service_.GetMemoryUsagePercentage();

  EXPECT_THAT(mem_usage_percentage.result(),
              ResultIs(FailureExecutionResult(
                  SC_PBS_HEALTH_SERVICE_COULD_NOT_OPEN_MEMINFO_FILE)));
}

TEST_F(HealthServiceTest,
       ShouldFailIfAnExpectedFieldIsMissingFromTheMemInfoFile) {
  // Set the path to the file to read meminfo from
  health_service_.SetMemInfoFilePath(
      "cc/pbs/health_service/test/files/missing_total_meminfo_file.txt");

  auto mem_usage_percentage = health_service_.GetMemoryUsagePercentage();
  EXPECT_EQ(
      mem_usage_percentage.result(),
      FailureExecutionResult(SC_PBS_HEALTH_SERVICE_COULD_NOT_FIND_MEMORY_INFO));

  // Set the path to the file to read meminfo from
  health_service_.SetMemInfoFilePath(
      "cc/pbs/health_service/test/files/missing_available_meminfo_file.txt");

  mem_usage_percentage = health_service_.GetMemoryUsagePercentage();
  EXPECT_EQ(
      mem_usage_percentage.result(),
      FailureExecutionResult(SC_PBS_HEALTH_SERVICE_COULD_NOT_FIND_MEMORY_INFO));
}

TEST_F(HealthServiceTest, ShouldFailIfMemInfoFileLineIsNotInTheExpectedFormat) {
  // Set the path to the file to read meminfo from
  health_service_.SetMemInfoFilePath(
      "cc/pbs/health_service/test/files/invalid_format_meminfo_file.txt");

  auto mem_usage_percentage = health_service_.GetMemoryUsagePercentage();
  EXPECT_THAT(mem_usage_percentage.result(),
              ResultIs(FailureExecutionResult(
                  SC_PBS_HEALTH_SERVICE_COULD_NOT_PARSE_MEMINFO_LINE)));
}

TEST_F(HealthServiceTest, ShouldFailHealthCheckIfReadingFromMemInfoFileFails) {
  // Set the path to the file to read meminfo from
  health_service_.SetMemInfoFilePath("file/that/does/not/exist.txt");

  AsyncContext<HttpRequest, HttpResponse> context;
  auto result = health_service_.CheckHealth(context);
  EXPECT_SUCCESS(result);
  // Request response fails
  EXPECT_FALSE(context.result.Successful());
}

TEST_F(HealthServiceTest,
       ShouldFailHealthCheckIfHealthyMemThresholdIsExceeded) {
  // Set the path to the file to read meminfo from
  health_service_.SetMemInfoFilePath(
      "cc/pbs/health_service/test/files/ninety_six_percent_meminfo_file.txt");

  AsyncContext<HttpRequest, HttpResponse> context;
  auto result = health_service_.CheckHealth(context);
  EXPECT_SUCCESS(result);
  // Request response fails
  EXPECT_EQ(context.result,
            FailureExecutionResult(
                SC_PBS_HEALTH_SERVICE_HEALTHY_MEMORY_USAGE_THRESHOLD_EXCEEDED));
}

TEST_F(HealthServiceTest, ShouldFailFsStoragePercentageIfReadingInfoFails) {
  health_service_.SetFileSystemSpaceInfo(FailureExecutionResult(
      SC_PBS_HEALTH_SERVICE_COULD_NOT_READ_FILESYSTEM_INFO));

  auto info = health_service_.GetFileSystemStorageUsagePercentage("dir");
  EXPECT_THAT(info.result(),
              ResultIs(FailureExecutionResult(
                  SC_PBS_HEALTH_SERVICE_COULD_NOT_READ_FILESYSTEM_INFO)));
}

TEST_F(HealthServiceTest, ShouldFailIfFsStorageInfoReadingIsInvalid) {
  space_info fs_space_info;
  fs_space_info.capacity = 0;
  fs_space_info.available = 50;
  health_service_.SetFileSystemSpaceInfo(fs_space_info);

  auto info = health_service_.GetFileSystemStorageUsagePercentage("dir");
  EXPECT_THAT(info.result(),
              ResultIs(FailureExecutionResult(
                  SC_PBS_HEALTH_SERVICE_INVALID_READ_FILESYSTEM_INFO)));

  fs_space_info.capacity = 50;
  fs_space_info.available = 0;
  health_service_.SetFileSystemSpaceInfo(fs_space_info);

  info = health_service_.GetFileSystemStorageUsagePercentage("dir");
  EXPECT_THAT(info.result(),
              ResultIs(FailureExecutionResult(
                  SC_PBS_HEALTH_SERVICE_INVALID_READ_FILESYSTEM_INFO)));
}

TEST_F(HealthServiceTest, ShouldGetFsStoragePercentage) {
  space_info fs_space_info;
  fs_space_info.capacity = 100;
  fs_space_info.available = 50;
  health_service_.SetFileSystemSpaceInfo(fs_space_info);

  auto percent = health_service_.GetFileSystemStorageUsagePercentage("dir");
  EXPECT_SUCCESS(percent.result());
  EXPECT_EQ(50, *percent);

  fs_space_info.capacity = 100;
  fs_space_info.available = 95;
  health_service_.SetFileSystemSpaceInfo(fs_space_info);

  percent = health_service_.GetFileSystemStorageUsagePercentage("dir");
  EXPECT_SUCCESS(percent.result());
  EXPECT_EQ(5, *percent);

  fs_space_info.capacity = 100;
  fs_space_info.available = 5;
  health_service_.SetFileSystemSpaceInfo(fs_space_info);

  percent = health_service_.GetFileSystemStorageUsagePercentage("dir");
  EXPECT_SUCCESS(percent.result());
  EXPECT_EQ(95, *percent);
}

TEST_F(HealthServiceTest,
       ShouldFailHealthCheckIfHealthyStorageThresholdIsExceeded) {
  space_info fs_space_info;
  // Results in 96% utilization
  fs_space_info.capacity = 100;
  fs_space_info.available = 4;
  health_service_.SetFileSystemSpaceInfo(fs_space_info);

  AsyncContext<HttpRequest, HttpResponse> context;
  auto result = health_service_.CheckHealth(context);
  EXPECT_SUCCESS(result);
  // Request response fails
  EXPECT_EQ(
      context.result,
      FailureExecutionResult(
          SC_PBS_HEALTH_SERVICE_HEALTHY_STORAGE_USAGE_THRESHOLD_EXCEEDED));
}

TEST_F(HealthServiceTest, ShouldFailHealthCheckIfFilesystemInfoCantBeRead) {
  health_service_.SetFileSystemSpaceInfo(FailureExecutionResult(SC_UNKNOWN));

  AsyncContext<HttpRequest, HttpResponse> context;
  auto result = health_service_.CheckHealth(context);
  EXPECT_SUCCESS(result);
  // Request response fails
  EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST_F(HealthServiceTest, OTelReturnsCorrectMemoryUsageInfo) {
  health_service_.SetMemInfoFilePath(
      "cc/pbs/health_service/test/files/ninety_six_percent_meminfo_file.txt");

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  std::optional<opentelemetry::sdk::metrics::PointType>
      memory_usage_metric_point_data = GetMetricPointData(
          privacy_sandbox::pbs::kMetricNameMemoryUsage, {}, data);
  ASSERT_TRUE(memory_usage_metric_point_data.has_value());

  auto memory_usage_last_value_point_data =
      std::move(std::get<opentelemetry::sdk::metrics::LastValuePointData>(
          memory_usage_metric_point_data.value()));
  EXPECT_EQ(std::get<int64_t>(memory_usage_last_value_point_data.value_), 96)
      << "Expected memory_usage_last_value_point_data.value_ to be 96 "
         "(int64_t)";
}

TEST_F(HealthServiceTest,
       OTelReturnsCorrectInstanceFileSystemStorageUsageInfo) {
  space_info fs_space_info;
  fs_space_info.capacity = 100;
  fs_space_info.available = 75;
  health_service_.SetFileSystemSpaceInfo(fs_space_info);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  std::optional<opentelemetry::sdk::metrics::PointType>
      filesystem_storage_usage_metric_point_data = GetMetricPointData(
          privacy_sandbox::pbs::kMetricNameFileSystemStorageUsage, {}, data);
  ASSERT_TRUE(filesystem_storage_usage_metric_point_data.has_value());

  auto filesystem_storage_usage_last_value_point_data =
      std::move(std::get<opentelemetry::sdk::metrics::LastValuePointData>(
          filesystem_storage_usage_metric_point_data.value()));
  EXPECT_EQ(
      std::get<int64_t>(filesystem_storage_usage_last_value_point_data.value_),
      25)
      << "Expected "
         "filesystem_storage_usage_last_value_point_data.value_ to be "
         "25 "
         "(int64_t)";
}

}  // namespace privacy_sandbox::pbs
