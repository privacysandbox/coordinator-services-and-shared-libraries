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

#include <vector>

#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class AwsMetricClientUtils {
 public:
  /**
   * @brief Parses MetricRecordRequest async context to Aws Metric Datum. In
   * this function, all bad requests will being filtered, like invalid
   * timestamp, oversize metric labels, invalid metric value.
   *
   * @param record_metric_context the async context for MetricRecordRequest.
   * @param datum_list AWS metric datum object list.
   * @param request_metric_limit Aws request metric limit.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult ParseRequestToDatum(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>&
          record_metric_context,
      std::vector<Aws::CloudWatch::Model::MetricDatum>& datum_list,
      int request_metric_limit) noexcept;
};

}  // namespace google::scp::cpio::client_providers
