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

#ifndef CC_CORE_TELEMETRY_MOCK_IN_MEMORY_METRIC_ROUTER_H_
#define CC_CORE_TELEMETRY_MOCK_IN_MEMORY_METRIC_ROUTER_H_

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/synchronization/notification.h"
#include "core/telemetry/mock/in_memory_metric_exporter.h"
#include "core/telemetry/mock/in_memory_metric_reader.h"
#include "core/telemetry/src/metric/metric_router.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

namespace google::scp::core {
class InMemoryMetricRouter : public MetricRouter {
 public:
  explicit InMemoryMetricRouter(
      bool is_otel_print_data_to_console_enabled = false);

  InMemoryMetricReader& GetMetricReader() const;
  InMemoryMetricExporter& GetMetricExporter() const;

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> GetExportedData()
      const;

 private:
  static std::shared_ptr<InMemoryMetricReader> CreateInMemoryReader(
      bool is_otel_print_data_to_console_enabled);

  std::shared_ptr<core::InMemoryMetricReader> metric_reader_;
};
}  // namespace google::scp::core
#endif
