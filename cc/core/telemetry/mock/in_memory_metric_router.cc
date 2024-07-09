//  Copyright 2024 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "core/telemetry/mock/in_memory_metric_router.h"

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/telemetry/mock/error_codes.h"
#include "core/telemetry/mock/in_memory_metric_exporter.h"
#include "core/telemetry/mock/in_memory_metric_reader.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace google::scp::core {
inline constexpr absl::string_view kInMemoryMetricRouter =
    "InMemoryMetricRouter";

InMemoryMetricRouter::InMemoryMetricRouter(
    bool is_otel_print_data_to_console_enabled)
    : metric_reader_(
          CreateInMemoryReader(is_otel_print_data_to_console_enabled)) {
  SetupMetricRouter(opentelemetry::sdk::resource::Resource::GetDefault(),
                    metric_reader_);
}

std::shared_ptr<InMemoryMetricReader>
InMemoryMetricRouter::CreateInMemoryReader(
    bool is_otel_print_data_to_console_enabled) {
  std::unique_ptr<InMemoryMetricExporter> exporter =
      std::make_unique<InMemoryMetricExporter>(
          is_otel_print_data_to_console_enabled);

  return std::make_shared<InMemoryMetricReader>(std::move(exporter));
}

InMemoryMetricReader& InMemoryMetricRouter::GetMetricReader() const {
  return *metric_reader_;
}

core::InMemoryMetricExporter& InMemoryMetricRouter::GetMetricExporter() const {
  return GetMetricReader().exporter();
}

std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
InMemoryMetricRouter::GetExportedData() const {
  if (GetMetricReader().ForceFlush()) {
    return GetMetricReader().GetExportedData();
  } else {
    auto execution_result =
        FailureExecutionResult(SC_TELEMETRY_FAKE_COULD_NOT_EXPORT_DATA);
    SCP_ERROR(kInMemoryMetricRouter, google::scp::core::common::kZeroUuid,
              execution_result, "[Telmetry Fake] Could force flush the data");

    return {};
  }
}

}  // namespace google::scp::core
