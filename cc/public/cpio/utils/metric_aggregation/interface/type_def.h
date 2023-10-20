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

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

#include "core/interface/type_def.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/type_def.h"

namespace google::scp::cpio {
using MetricNamespace = std::string;
using MetricName = std::string;
using MetricValue = std::string;
using MetricLabels = std::map<std::string, std::string>;
/// Supported metric units.
enum class MetricUnit {
  kSeconds = 1,
  kMicroseconds = 2,
  kMilliseconds = 3,
  kBits = 4,
  kKilobits = 5,
  kMegabits = 6,
  kGigabits = 7,
  kTerabits = 8,
  kBytes = 9,
  kKilobytes = 10,
  kMegabytes = 11,
  kGigabytes = 12,
  kTerabytes = 13,
  kCount = 14,
  kPercent = 15,
  kBitsPerSecond = 16,
  kKilobitsPerSecond = 17,
  kMegabitsPerSecond = 18,
  kGigabitsPerSecond = 19,
  kTerabitsPerSecond = 20,
  kBytesPerSecond = 21,
  kKilobytesPerSecond = 22,
  kMegabytesPerSecond = 23,
  kGigabytesPerSecond = 24,
  kTerabytesPerSecond = 25,
  kCountPerSecond = 26,
};

static constexpr MetricUnit kCountUnit = MetricUnit::kCount;
static constexpr MetricUnit kCountSecond = MetricUnit::kCountPerSecond;
static constexpr MetricUnit kMillisecondsUnit = MetricUnit::kMilliseconds;

static constexpr char kMethodName[] = "MethodName";
static constexpr char kComponentName[] = "ComponentName";

/// Builds a metric name with component name and event name.
struct MetricLabelsBase {
  MetricLabelsBase(const std::string& component_name = std::string(),
                   const std::string& method_name = std::string())
      : component_name(component_name), method_name(method_name) {}

  /**
   * @brief Get the Basic Labels object
   *
   * @return MetricLabels the basic metric labels object.
   */
  MetricLabels GetMetricLabelsBase() {
    MetricLabels metric_labels;
    if (!method_name.empty()) {
      metric_labels[kMethodName] = method_name;
    }
    if (!component_name.empty()) {
      metric_labels[kComponentName] = component_name;
    }
    return metric_labels;
  }

  /// The component name of the metric instance.
  std::string component_name;
  /// The method name of the metric instance.
  std::string method_name;
};

/// Represents the metric definition.
struct MetricDefinition {
  explicit MetricDefinition(const std::shared_ptr<MetricName>& metric_name,
                            const std::shared_ptr<MetricUnit>& metric_unit)
      : name(metric_name), unit(metric_unit) {}

  /// Metric name.
  std::shared_ptr<MetricName> name;
  /// Metric unit.
  std::shared_ptr<MetricUnit> unit;
  /// A set of key-value pairs. The key represents label name and the value
  /// represents label value.
  std::shared_ptr<MetricLabels> labels;
  /// The namespace parameter required for pushing metric data to AWS.
  std::shared_ptr<MetricNamespace> name_space;
};

/**
 * @brief The structure of TimeEvent which used to record the start time, end
 * time and difference time for one event.
 *
 */
struct TimeEvent {
  /**
   * @brief Construct a new Time Event object. The start_time is the time when
   * object constructed.
   */
  TimeEvent() {
    start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch())
                     .count();
  }

  /**
   * @brief When Stop() is called, the end_time is recorded and also the
   * diff_time is the difference between end_time and start_time.
   */
  void Stop() {
    end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    diff_time = end_time - start_time;
  }

  /// The start time for the event.
  core::Timestamp start_time;
  /// The end time for the event.
  core::Timestamp end_time;
  /// The duration time for the event.
  core::TimeDuration diff_time;
};

/**
 * @brief The metric tag for the specific definition of a metric. The metric tag
 * is used to override the metric name and unit defined before, and also add
 * more labels to the metric.
 *
 */
struct MetricTag {
  explicit MetricTag(
      const std::shared_ptr<MetricName>& update_name = nullptr,
      const std::shared_ptr<MetricUnit>& update_unit = nullptr,
      const std::shared_ptr<MetricLabels>& additional_labels = nullptr)
      : update_name(update_name),
        update_unit(update_unit),
        additional_labels(additional_labels) {}

  /// Updates metric name for one metric.
  std::shared_ptr<MetricName> update_name;
  /// Updates metric unit for one metric.
  std::shared_ptr<MetricUnit> update_unit;
  /// The additional labels for metric identification.
  std::shared_ptr<MetricLabels> additional_labels;
};
}  // namespace google::scp::cpio
