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

#ifndef CC_CORE_TELEMETRY_SRC_AUTHENTICATION_METRIC_UTILS
#define CC_CORE_TELEMETRY_SRC_AUTHENTICATION_METRIC_UTILS

#include <optional>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"

namespace google::scp::core {

/*
 * Simplified overview of OpenTelemetry metric data structure
 *
 * ResourceMetrics
 *     ├─ Resource
 *     └─ std::vector<ScopeMetrics>
 *                     ├─ InstrumentationScope
 *                     └─ std::vector<MetricData>
 *                                     ├─ InstrumentDescriptor
 *                                     └─ std::vector<PointDataAttributes>
 *                                                     ├─ PointAttributes
 *                                                     └─ PointType
 */

/**
 * @brief Get a PointType metric point data from a ResourceMetrics metric data
 * source matching the exact instrument name and containing the metric
 * attributes supplied as a subset.
 *
 * @param name Instrument name to be matched.
 * @param dimensions Metric attributes to be matched as subset.
 * @param data Source of metric data.
 * @return opentelemetry::sdk::metrics::PointType, normally;
 *         std::nullopt, if a match cannot be found.
 */
std::optional<opentelemetry::sdk::metrics::PointType> GetMetricPointData(
    absl::string_view name,
    const opentelemetry::sdk::common::OrderedAttributeMap& dimensions,
    absl::Span<const opentelemetry::sdk::metrics::ResourceMetrics> data);

/**
 * @brief Get the OrderedAttributeMap metric attributes from a ResourceMetrics
 * metric data source matching the exact instrument name.
 *
 * @param name Instrument name to be matched.
 * @param data Source of metric data.
 * @return opentelemetry::sdk::common::OrderedAttributeMap, normally;
 *         std::nullopt, if a match cannot be found.
 */
std::optional<opentelemetry::sdk::common::OrderedAttributeMap>
GetMetricAttributes(
    absl::string_view name,
    absl::Span<const opentelemetry::sdk::metrics::ResourceMetrics> data);

}  // namespace google::scp::core

#endif
