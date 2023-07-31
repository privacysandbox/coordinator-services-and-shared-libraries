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
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include <google/protobuf/map.h>

#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "cpio/client_providers/metric_client_provider/src/metric_client_utils.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
class MetricUtils {
 public:
  static void GetPutMetricsRequest(
      std::shared_ptr<cmrt::sdk::metric_service::v1::PutMetricsRequest>&
          record_metric_request,
      const std::shared_ptr<MetricDefinition>& metric_info,
      const std::shared_ptr<MetricValue>& metric_value,
      const std::shared_ptr<MetricTag>& metric_tag = nullptr) noexcept {
    auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    auto metric = record_metric_request->add_metrics();
    metric->set_value(*metric_value);
    auto final_name = (metric_tag && metric_tag->update_name)
                          ? *metric_tag->update_name
                          : *metric_info->name;
    metric->set_name(final_name);
    auto final_unit = (metric_tag && metric_tag->update_unit)
                          ? *metric_tag->update_unit
                          : *metric_info->unit;
    metric->set_unit(MetricClientUtils::ConvertToMetricUnitProto(final_unit));

    // Adds the labels from metric_info and additional_labels.
    auto labels = metric->mutable_labels();
    if (metric_info->labels) {
      for (const auto& label : *metric_info->labels) {
        labels->insert(protobuf::MapPair(label.first, label.second));
      }
    }
    if (metric_tag && metric_tag->additional_labels) {
      for (const auto& label : *metric_tag->additional_labels) {
        labels->insert(protobuf::MapPair(label.first, label.second));
      }
    }
    metric->set_timestamp_in_ms(current_time);
  }
};

}  // namespace google::scp::cpio::client_providers
