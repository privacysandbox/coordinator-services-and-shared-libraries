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
#include <string>
#include <vector>

#include "opentelemetry/sdk/metrics/export/metric_producer.h"

namespace google::scp::core {

std::optional<opentelemetry::sdk::metrics::PointType> GetMetricPointData(
    const std::string& name,
    const opentelemetry::sdk::common::OrderedAttributeMap& dimensions,
    const std::vector<opentelemetry::sdk::metrics::ResourceMetrics>& data);

}  // namespace google::scp::core

#endif
