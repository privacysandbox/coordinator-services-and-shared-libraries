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

#include <functional>
#include <iostream>
#include <vector>

#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"

namespace {

/**
 * @brief Get an array of PointDataAttributes metric data from a ResourceMetrics
 * metric data source matching the exact instrument name.
 *
 * @param name Instrument name to be matched.
 * @param data Source of metric data.
 * @return absl::Span<const PointDataAttributes>, normally;
 *         an empty span, if a match cannot be found.
 */
absl::Span<const opentelemetry::sdk::metrics::PointDataAttributes>
GetMetricPointDataAttrsArr(
    absl::string_view name,
    absl::Span<const opentelemetry::sdk::metrics::ResourceMetrics> data) {
  if (data.empty()) {
    return absl::Span<const opentelemetry::sdk::metrics::PointDataAttributes>{};
  }

  // We take the first ResourceMetrics from data. It should also be the only one
  // associated with the current Resource.
  const opentelemetry::sdk::metrics::ResourceMetrics& resource_metrics =
      data[0];

  for (const opentelemetry::sdk::metrics::ScopeMetrics& scope_metrics :
       resource_metrics.scope_metric_data_) {
    for (const opentelemetry::sdk::metrics::MetricData& metric :
         scope_metrics.metric_data_) {
      const opentelemetry::sdk::metrics::InstrumentDescriptor&
          instrument_descriptor = metric.instrument_descriptor;
      if (instrument_descriptor.name_ == name) {
        return metric.point_data_attr_;
      }
    }
  }

  return absl::Span<const opentelemetry::sdk::metrics::PointDataAttributes>{};
}

}  // namespace

namespace google::scp::core {

/*
 * Sample exported data
 * {
 *   scope name	: test_meter
 *   schema url	: dummy_schema_url
 *   version	    : 1
 *   start time	: Wed Feb 28 01:25:05 2024
 *   end time	  : Wed Feb 28 01:25:07 2024
 *   instrument name	: test_counter
 *   description	: test_counter_description
 *   unit		    :
 *   type		    : SumPointData
 *   value		    : 30
 *   attributes	:
 *     attribute1: value1
 *     attribute2: 42
 *   resources	  :
 *     service.name: unknown_service
 *     telemetry.sdk.language: cpp
 *     telemetry.sdk.name: opentelemetry
 *     telemetry.sdk.version: 1.13.0
 * }
 */
std::optional<opentelemetry::sdk::metrics::PointType> GetMetricPointData(
    absl::string_view name,
    const opentelemetry::sdk::common::OrderedAttributeMap& dimensions,
    absl::Span<const opentelemetry::sdk::metrics::ResourceMetrics> data) {
  auto point_data_attrs = GetMetricPointDataAttrsArr(name, data);

  for (const opentelemetry::sdk::metrics::PointDataAttributes& point_data_attr :
       point_data_attrs) {
    const opentelemetry::sdk::common::OrderedAttributeMap& attributes =
        point_data_attr.attributes;
    bool is_subset = true;
    // If every entry in the `dimensions` attributes map can
    // be found in `attributes`, then a subset match is found.
    //
    // Similarly, if the supplied `dimensions` is empty, a subset match
    // is also found.
    for (const auto& dimension : dimensions) {
      is_subset = attributes.find(dimension.first) != attributes.end();
      if (is_subset) {
        is_subset = attributes.at(dimension.first) == dimension.second;
      }
      if (!is_subset) {
        break;
      }
    }
    if (is_subset) {
      return point_data_attr.point_data;
    }
  }

  return std::nullopt;
}

std::optional<opentelemetry::sdk::common::OrderedAttributeMap>
GetMetricAttributes(
    absl::string_view name,
    absl::Span<const opentelemetry::sdk::metrics::ResourceMetrics> data) {
  auto point_data_attrs = GetMetricPointDataAttrsArr(name, data);
  if (point_data_attrs.empty()) {
    return std::nullopt;
  }

  // Return the OrderedAttributeMap of the first PointDataAttributes
  // in `point_data_attrs` matching the instrument name.
  return point_data_attrs[0].attributes;
}

}  // namespace google::scp::core
