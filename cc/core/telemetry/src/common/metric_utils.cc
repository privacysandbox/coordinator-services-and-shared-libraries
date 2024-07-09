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

#include "metric_utils.h"

#include <iostream>
#include <vector>

#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"

namespace google::scp::core {

/*
* Sample exported data
* {
scope name	: test_meter
schema url	: dummy_schema_url
version	        : 1
start time	: Wed Feb 28 01:25:05 2024
end time	: Wed Feb 28 01:25:07 2024
instrument name	: test_counter
description	: test_counter_description
unit		:
type		: SumPointData
value		: 30
attributes	:
  attribute1: value1
  attribute2: 42
resources	:
  service.name: unknown_service
  telemetry.sdk.language: cpp
  telemetry.sdk.name: opentelemetry
  telemetry.sdk.version: 1.13.0
}
*/
std::optional<opentelemetry::sdk::metrics::PointType> GetMetricPointData(
    const std::string& name,
    const opentelemetry::sdk::common::OrderedAttributeMap& dimensions,
    const std::vector<opentelemetry::sdk::metrics::ResourceMetrics>& data) {
  for (const auto& resource_metrics : data) {
    for (const auto& scope_metrics : resource_metrics.scope_metric_data_) {
      for (const auto& metric : scope_metrics.metric_data_) {
        const auto& instrument_descriptor = metric.instrument_descriptor;
        if (instrument_descriptor.name_ == name) {
          for (const auto& point_data_attr : metric.point_data_attr_) {
            if (point_data_attr.attributes == dimensions) {
              return point_data_attr.point_data;
            }
          }
        }
      }
    }
  }
  return std::nullopt;
}
}  // namespace google::scp::core
