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

#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cc/pbs/health_service/src/error_codes.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/type_def.h"
#include "opentelemetry/metrics/provider.h"

using absl::SimpleAtoi;
using absl::SkipEmpty;
using absl::StrContains;
using absl::StrSplit;
using ::privacy_sandbox::pbs_common::AsyncContext;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::ExecutionResultOr;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::HttpHandler;
using ::privacy_sandbox::pbs_common::HttpMethod;
using ::privacy_sandbox::pbs_common::HttpRequest;
using ::privacy_sandbox::pbs_common::HttpResponse;
using ::privacy_sandbox::pbs_common::kZeroUuid;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;

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

namespace privacy_sandbox::pbs {

// static
void HealthService::ObserveMemoryUsageCallback(
    opentelemetry::metrics::ObserverResult observer_result,
    absl::Nonnull<HealthService*> self_ptr) {
  auto observer = std::get<opentelemetry::nostd::shared_ptr<
      opentelemetry::metrics::ObserverResultT<int64_t>>>(observer_result);
  observer->Observe(*(self_ptr->GetMemoryUsagePercentage()));
}

// static
void HealthService::ObserveFileSystemStorageUsageCallback(
    opentelemetry::metrics::ObserverResult observer_result,
    absl::Nonnull<HealthService*> self_ptr) {
  auto observer = std::get<opentelemetry::nostd::shared_ptr<
      opentelemetry::metrics::ObserverResultT<int64_t>>>(observer_result);
  observer->Observe(
      *(self_ptr->GetFileSystemStorageUsagePercentage(kVarLogDirectory)));
}

HealthService::~HealthService() {
  if (memory_usage_instrument_) {
    memory_usage_instrument_->RemoveCallback(
        reinterpret_cast<opentelemetry::metrics::ObservableCallbackPtr>(
            &HealthService::ObserveMemoryUsageCallback),
        this);
  }
  if (filesystem_storage_usage_instrument_) {
    filesystem_storage_usage_instrument_->RemoveCallback(
        reinterpret_cast<opentelemetry::metrics::ObservableCallbackPtr>(
            &HealthService::ObserveFileSystemStorageUsageCallback),
        this);
  }
}

ExecutionResult HealthService::Init() noexcept {
  HttpHandler check_health_handler =
      std::bind(&HealthService::CheckHealth, this, std::placeholders::_1);
  std::string resource_path("/health");
  http_server_->RegisterResourceHandler(HttpMethod::GET, resource_path,
                                        check_health_handler);

  if (PerformMemoryAndStorageUsageCheck()) {
    SCP_DEBUG(kServiceName, kZeroUuid,
              "Perform active memory and storage check: YES");
  } else {
    SCP_DEBUG(kServiceName, kZeroUuid,
              "Perform active memory and storage check: NO");
  }

  meter_ = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
      "HealthService");
  memory_usage_instrument_ = meter_->CreateInt64ObservableGauge(
      privacy_sandbox::pbs::kMetricNameMemoryUsage, "Instance memory usage",
      "percent");
  filesystem_storage_usage_instrument_ = meter_->CreateInt64ObservableGauge(
      privacy_sandbox::pbs::kMetricNameFileSystemStorageUsage,
      "Instance file system storage usage", "percent");

  memory_usage_instrument_->AddCallback(
      reinterpret_cast<opentelemetry::metrics::ObservableCallbackPtr>(
          &HealthService::ObserveMemoryUsageCallback),
      this);
  filesystem_storage_usage_instrument_->AddCallback(
      reinterpret_cast<opentelemetry::metrics::ObservableCallbackPtr>(
          &HealthService::ObserveFileSystemStorageUsageCallback),
      this);

  return SuccessExecutionResult();
}

ExecutionResult HealthService::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult HealthService::Stop() noexcept {
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

std::string HealthService::GetMemInfoFilePath() noexcept {
  return std::string(kMemInfoFileName);
}

/**
 * @brief Parse a meminfo line and read the numeric value.
 *
 * @param meminfo_line A std::string representing a line in the meminfo file.
 * line.
 * @return ExecutionResultOr<uint64_t>
 */
static ExecutionResultOr<uint64_t> GetMemInfoLineEntryKb(
    std::string meminfo_line) {
  std::vector<std::string> line_parts =
      StrSplit(meminfo_line, kMemInfoLineSeparator, SkipEmpty());

  if (line_parts.size() != kExpectedMemInfoLinePartsCount) {
    return FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_COULD_NOT_PARSE_MEMINFO_LINE);
  }

  uint64_t read_memory_kb;
  std::stringstream int_parsing_stream;
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
  std::ifstream meminfo_file(meminfo_file_path);
  if (meminfo_file.fail()) {
    return FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_COULD_NOT_OPEN_MEMINFO_FILE);
  }
  // The contents of this file looks like:
  // MemTotal:       198065040 kB
  // MemAvailable:   185711588 kB
  // ...
  // So we parse the lines of interest to compute the used percentage.

  std::string line = "";
  uint64_t total_usable_mem_kb = 0;
  uint64_t total_available_mem_kb = 0;

  while (std::getline(meminfo_file, line)) {
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
            absl::StrFormat(
                "Memory : { \"total\": \"%lu kb\", \"available\": \"%lu kb\" }",
                total_usable_mem_kb, total_available_mem_kb));

  return ComputePercentage(total_available_mem_kb, total_usable_mem_kb);
}

ExecutionResultOr<std::filesystem::space_info>
HealthService::GetFileSystemSpaceInfo(std::string directory) noexcept {
  std::error_code ec;
  std::filesystem::space_info info = std::filesystem::space(directory, ec);
  if (ec) {
    auto result = FailureExecutionResult(
        SC_PBS_HEALTH_SERVICE_COULD_NOT_READ_FILESYSTEM_INFO);
    SCP_ERROR(kServiceName, kZeroUuid, result,
              absl::StrFormat("Failed to read the filesystem information: %s",
                              ec.message()));
    return result;
  }

  return info;
}

ExecutionResultOr<int> HealthService::GetFileSystemStorageUsagePercentage(
    const std::string& directory) noexcept {
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
            absl::StrFormat(
                "Storage : { \"total\": \"%lu b\", \"available\": \"%lu b\" }",
                info_object.capacity, info_object.available));

  return ComputePercentage(info_object.available, info_object.capacity);
}

}  // namespace privacy_sandbox::pbs
