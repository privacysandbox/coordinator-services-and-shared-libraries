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

#include <memory>

#include "public/core/interface/execution_result.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Provides a simple metric service. It pushes the single metric
 * data point for the target server to the cloud server.
 */
class SimpleMetricInterface : public core::ServiceInterface {
 public:
  virtual ~SimpleMetricInterface() = default;
  /**
   * @brief Schedules a metric push.
   *
   * @param metric_value the metric value used to construct a
   * RecordMetricRequest proto message with input metric_value and current
   * timestamp.
   */

  /**
   * @brief Schedules a simple metric push.
   *
   * @param metric_value The metric value to be pushed.
   * @param metric_tag The metric tag for specific simple metric definition. The
   * metric tag will override pervious metric definition.
   */
  virtual void Push(
      const std::shared_ptr<MetricValue>& metric_value,
      const std::shared_ptr<MetricTag>& metric_tag = nullptr) noexcept = 0;
};

}  // namespace google::scp::cpio
