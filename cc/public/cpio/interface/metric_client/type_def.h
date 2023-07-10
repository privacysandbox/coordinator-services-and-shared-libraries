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

#ifndef SCP_CPIO_INTERFACE_METRIC_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_METRIC_CLIENT_TYPE_DEF_H_

#include <chrono>
#include <map>
#include <string>
#include <vector>

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

/// Configurations for MetricClient.
struct MetricClientOptions {
  virtual ~MetricClientOptions() = default;

  /**
   * @brief Required. The top level grouping for the application metrics. A
   * typical example would be "/application_name/environment_name".
   * Environment name could be fetched through ConfigClient.
   *
   */
  MetricNamespace metric_namespace;
  /**
   * @brief Pushes metrics in batches if true. In most times, when the
   * batch_recording_time_duration is met, the push is triggered. Cloud has
   * its own maximum batch size, and if the maximum batch size is met before the
   * batch_recording_time_duration, the push is triggered too.
   */
  bool enable_batch_recording = false;
  /**
   * @brief The time duration to push metrics when enable_batch_recording is
   * true.
   */
  std::chrono::milliseconds batch_recording_time_duration =
      std::chrono::milliseconds(30000);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_METRIC_CLIENT_TYPE_DEF_H_
