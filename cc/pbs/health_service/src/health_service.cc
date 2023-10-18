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

#include "health_service.h"

#include <fstream>
#include <functional>
#include <sstream>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "core/common/time_provider/src/time_provider.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/metrics_def.h"
#include "public/cpio/utils/metric_aggregation/interface/simple_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/src/metric_utils.h"
#include "public/cpio/utils/metric_aggregation/src/simple_metric.h"

#include "error_codes.h"

using absl::SimpleAtoi;
using absl::SkipEmpty;
using absl::StrContains;
using absl::StrSplit;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHandler;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_PBS_HEALTH_SERVICE_COULD_NOT_FIND_MEMORY_INFO;
using google::scp::core::errors::
    SC_PBS_HEALTH_SERVICE_COULD_NOT_OPEN_MEMINFO_FILE;
using google::scp::core::errors::
    SC_PBS_HEALTH_SERVICE_COULD_NOT_PARSE_MEMINFO_LINE;
using google::scp::core::errors::
    SC_PBS_HEALTH_SERVICE_COULD_NOT_READ_FILESYSTEM_INFO;
using google::scp::core::errors::
    SC_PBS_HEALTH_SERVICE_HEALTHY_MEMORY_USAGE_THRESHOLD_EXCEEDED;
using google::scp::core::errors::
    SC_PBS_HEALTH_SERVICE_HEALTHY_STORAGE_USAGE_THRESHOLD_EXCEEDED;
using google::scp::core::errors::
    SC_PBS_HEALTH_SERVICE_INVALID_READ_FILESYSTEM_INFO;
using google::scp::cpio::kCountUnit;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricLabelsBase;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using google::scp::cpio::MetricUtils;
using google::scp::cpio::MetricValue;
using google::scp::pbs::kPBSHealthServiceEnableMemoryAndStorageCheck;
using std::bind;
using std::error_code;
using std::getline;
using std::ifstream;
using std::make_shared;
using std::string;
using std::stringstream;
using std::to_string;
using std::vector;
using std::chrono::seconds;
using std::filesystem::space;
using std::filesystem::space_info;
using std::placeholders::_1;

static constexpr int kExpectedMemInfoLinePartsCount = 3;
static constexpr uint64_t kMemoryUsagePercentageHealthyThreshold = 95;
static constexpr uint64_t kFileSystemStorageUsagePercentageHealthyThreshold =
    95;
static constexpr int kExpectedMemInfoLineNumericValueIndex = 1;
static constexpr char kTotalUsableMemory[] = "MemTotal";
static constexpr char kTotalAvailableMemory[] = "MemAvailable";
static constexpr char kMemInfoFileName[] = "/proc/meminfo";
static constexpr char kMemInfoLineSeparator[] = " ";
static constexpr char kServiceName[] = "HealthCheckService";
static constexpr char kVarLogDirectory[] = "/var/log";

static constexpr size_t kDefaultInstanceHealthMetricPushIntervalInSeconds = 10;

namespace google::scp::pbs {
ExecutionResult HealthService::Init() noexcept {
  HttpHandler check_health_handler =
      bind(&HealthService::CheckHealth, this, _1);
  string resource_path("/health");
  http_server_->RegisterResourceHandler(HttpMethod::GET, resource_path,
                                        check_health_handler);

  if (PerformMemoryAndStorageUsageCheck()) {
    SCP_DEBUG(kServiceName, kZeroUuid,
              "Perform active memory and storage check: YES");
  } else {
    SCP_DEBUG(kServiceName, kZeroUuid,
              "Perform active memory and storage check: NO");
  }

  instance_memory_usage_metric_ = MetricUtils::RegisterSimpleMetric(
      async_executor_, metric_client_, kMetricNameInstanceHealthMemory,
      kMetricComponentNameInstanceHealth, kMetricComponentNameInstanceHealth,
      kCountUnit);

  instance_filesystem_storage_usage_metric_ = MetricUtils::RegisterSimpleMetric(
      async_executor_, metric_client_,
      kMetricNameInstanceHealthFileSystemLogStorageUsage,
      kMetricComponentNameInstanceHealth, kMetricComponentNameInstanceHealth,
      kCountUnit);

  RETURN_IF_FAILURE(instance_memory_usage_metric_->Init());
  RETURN_IF_FAILURE(instance_filesystem_storage_usage_metric_->Init());
  return SuccessExecutionResult();
}

ExecutionResult HealthService::Run() noexcept {
  RETURN_IF_FAILURE(instance_memory_usage_metric_->Run());
  RETURN_IF_FAILURE(instance_filesystem_storage_usage_metric_->Run());
  return SuccessExecutionResult();
}

ExecutionResult HealthService::Stop() noexcept {
  RETURN_IF_FAILURE(instance_memory_usage_metric_->Stop());
  RETURN_IF_FAILURE(instance_filesystem_storage_usage_metric_->Stop());
  return SuccessExecutionResult();
}

ExecutionResult HealthService::CheckHealth(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  // Only do the memory and storage check if this config is enabled
  if (PerformMemoryAndStorageUsageCheck()) {
    auto result = CheckMemoryAndStorageUsage();
    if (!result.Successful()) {
      http_context.result = result;
      http_context.Finish();
      return SuccessExecutionResult();
    }
  }

  http_context.result = SuccessExecutionResult();
  http_context.Finish();
  return SuccessExecutionResult();
}

ExecutionResult HealthService::CheckMemoryAndStorageUsage() noexcept {
  ExecutionResult result = SuccessExecutionResult();

  auto used_memory_percentage = GetMemoryUsagePercentage();
  if (!used_memory_percentage.Successful()) {
    result = used_memory_percentage.result();
    SCP_CRITICAL(kServiceName, kZeroUuid, result,
                 "Failed to read memory info from meminfo file.");
    return result;
  }

  auto used_storage_percentage =
      GetFileSystemStorageUsagePercentage(kVarLogDirectory);
  if (!used_storage_percentage.result().Successful()) {
    result = used_storage_percentage.result();
    SCP_CRITICAL(kServiceName, kZeroUuid, result,
                 "Failed to read filesystem storage info.");
    return result;
  }

  // Emit metric.
  if (TimeProvider::GetSteadyTimestampInNanoseconds() -
          last_metric_push_steady_ns_timestamp_ >
      seconds(kDefaultInstanceHealthMetricPushIntervalInSeconds)) {
    instance_memory_usage_metric_->Push(
        make_shared<MetricValue>(to_string(*used_memory_percentage)));
    instance_filesystem_storage_usage_metric_->Push(
        make_shared<MetricValue>(to_string(*used_storage_percentage)));
    last_metric_push_steady_ns_timestamp_ =
        TimeProvider::GetSteadyTimestampInNanoseconds();
  }

  if (*used_memory_percentage > kMemoryUsagePercentageHealthyThreshold) {
    result = FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_HEALTHY_MEMORY_USAGE_THRESHOLD_EXCEEDED);
    SCP_CRITICAL(kServiceName, kZeroUuid, result,
                 "Healthy memory usage threshold was exceeded.");
    return result;
  }

  if (*used_storage_percentage >
      kFileSystemStorageUsagePercentageHealthyThreshold) {
    result = FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_HEALTHY_STORAGE_USAGE_THRESHOLD_EXCEEDED);
    SCP_CRITICAL(kServiceName, kZeroUuid, result,
                 "Healthy storage usage threshold was exceeded.");
    return result;
  }

  return result;
}

bool HealthService::PerformMemoryAndStorageUsageCheck() noexcept {
  bool check_mem_and_storage = false;
  auto config_exists = config_provider_
                           ->Get(kPBSHealthServiceEnableMemoryAndStorageCheck,
                                 check_mem_and_storage)
                           .Successful();
  return config_exists && check_mem_and_storage;
}

string HealthService::GetMemInfoFilePath() noexcept {
  return string(kMemInfoFileName);
}

/**
 * @brief Parse a meminfo line and read the numeric value.
 *
 * @param meminfo_line A string representing a line in the meminfo file.
 * line.
 * @return ExecutionResultOr<uint64_t>
 */
static ExecutionResultOr<uint64_t> GetMemInfoLineEntryKb(string meminfo_line) {
  vector<string> line_parts =
      StrSplit(meminfo_line, kMemInfoLineSeparator, SkipEmpty());

  if (line_parts.size() != kExpectedMemInfoLinePartsCount) {
    return FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_COULD_NOT_PARSE_MEMINFO_LINE);
  }

  uint64_t read_memory_kb;
  stringstream int_parsing_stream;
  int_parsing_stream << line_parts.at(kExpectedMemInfoLineNumericValueIndex);
  int_parsing_stream >> read_memory_kb;

  if (!SimpleAtoi(line_parts.at(kExpectedMemInfoLineNumericValueIndex),
                  &read_memory_kb)) {
    return FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_COULD_NOT_PARSE_MEMINFO_LINE);
  }

  return read_memory_kb;
}

static int ComputePercentage(uint64_t partial, uint64_t total) {
  double percentage = 1.0 - (partial / static_cast<double>(total));
  return static_cast<int>(percentage * 100);
}

ExecutionResultOr<int> HealthService::GetMemoryUsagePercentage() noexcept {
  auto meminfo_file_path = GetMemInfoFilePath();
  ifstream meminfo_file(meminfo_file_path);
  if (meminfo_file.fail()) {
    return FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_COULD_NOT_OPEN_MEMINFO_FILE);
  }
  // The contents of this file looks like:
  // MemTotal:       198065040 kB
  // MemAvailable:   185711588 kB
  // ...
  // So we parse the lines of interest to compute the used percentage.

  string line = "";
  uint64_t total_usable_mem_kb = 0;
  uint64_t total_available_mem_kb = 0;

  while (getline(meminfo_file, line)) {
    if (StrContains(line, kTotalUsableMemory)) {
      auto mem_value = GetMemInfoLineEntryKb(line);
      if (mem_value.Successful()) {
        total_usable_mem_kb = *mem_value;
      } else {
        return mem_value.result();
      }
    }

    if (StrContains(line, kTotalAvailableMemory)) {
      auto mem_value = GetMemInfoLineEntryKb(line);
      if (mem_value.Successful()) {
        total_available_mem_kb = *mem_value;
      } else {
        return mem_value.result();
      }
    }
  }

  if (total_usable_mem_kb < 1 || total_available_mem_kb < 1) {
    return FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_COULD_NOT_FIND_MEMORY_INFO);
  }

  SCP_DEBUG(kServiceName, kZeroUuid,
            "Memory : { \"total\": \"%lu kb\", \"available\": \"%lu kb\" }",
            total_usable_mem_kb, total_available_mem_kb);

  return ComputePercentage(total_available_mem_kb, total_usable_mem_kb);
}

ExecutionResultOr<space_info> HealthService::GetFileSystemSpaceInfo(
    string directory) noexcept {
  error_code ec;
  space_info info = space(directory, ec);
  if (ec) {
    auto result = FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_COULD_NOT_READ_FILESYSTEM_INFO);
    SCP_ERROR(kServiceName, kZeroUuid, result,
              "Failed to read the filesystem information: %s",
              ec.message().c_str());
    return result;
  }

  return info;
}

ExecutionResultOr<int> HealthService::GetFileSystemStorageUsagePercentage(
    const string& directory) noexcept {
  auto info = GetFileSystemSpaceInfo(directory);
  if (!info.result().Successful()) {
    return info.result();
  }

  auto info_object = *info;
  if (info_object.available < 1 || info_object.capacity < 1) {
    return FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_INVALID_READ_FILESYSTEM_INFO);
  }

  SCP_DEBUG(kServiceName, kZeroUuid,
            "Storage : { \"total\": \"%lu b\", \"available\": \"%lu b\" }",
            info_object.capacity, info_object.available);

  return ComputePercentage(info_object.available, info_object.capacity);
}
}  // namespace google::scp::pbs
