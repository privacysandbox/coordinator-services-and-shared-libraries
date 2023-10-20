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

#include "metric_client_utils.h"

#include <map>
#include <memory>

#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/src/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NAME_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_VALUE_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET;
using google::scp::cpio::MetricUnit;
using std::shared_ptr;

static const std::map<MetricUnit,
                      google::cmrt::sdk::metric_service::v1::MetricUnit>
    kMetricUnitMap = {
        {MetricUnit::kSeconds, google::cmrt::sdk::metric_service::v1::
                                   MetricUnit::METRIC_UNIT_SECONDS},
        {MetricUnit::kMicroseconds, google::cmrt::sdk::metric_service::v1::
                                        MetricUnit::METRIC_UNIT_MICROSECONDS},
        {MetricUnit::kMilliseconds, google::cmrt::sdk::metric_service::v1::
                                        MetricUnit::METRIC_UNIT_MILLISECONDS},
        {MetricUnit::kBits,
         google::cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_BITS},
        {MetricUnit::kKilobits, google::cmrt::sdk::metric_service::v1::
                                    MetricUnit::METRIC_UNIT_KILOBITS},
        {MetricUnit::kMegabits, google::cmrt::sdk::metric_service::v1::
                                    MetricUnit::METRIC_UNIT_MEGABITS},
        {MetricUnit::kGigabits, google::cmrt::sdk::metric_service::v1::
                                    MetricUnit::METRIC_UNIT_GIGABITS},
        {MetricUnit::kTerabits, google::cmrt::sdk::metric_service::v1::
                                    MetricUnit::METRIC_UNIT_TERABITS},
        {MetricUnit::kBytes,
         google::cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_BYTES},
        {MetricUnit::kKilobytes, google::cmrt::sdk::metric_service::v1::
                                     MetricUnit::METRIC_UNIT_KILOBYTES},
        {MetricUnit::kMegabytes, google::cmrt::sdk::metric_service::v1::
                                     MetricUnit::METRIC_UNIT_MEGABYTES},
        {MetricUnit::kGigabytes, google::cmrt::sdk::metric_service::v1::
                                     MetricUnit::METRIC_UNIT_GIGABYTES},
        {MetricUnit::kTerabytes, google::cmrt::sdk::metric_service::v1::
                                     MetricUnit::METRIC_UNIT_TERABYTES},
        {MetricUnit::kCount,
         google::cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_COUNT},
        {MetricUnit::kPercent, google::cmrt::sdk::metric_service::v1::
                                   MetricUnit::METRIC_UNIT_PERCENT},
        {MetricUnit::kBitsPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_BITS_PER_SECOND},
        {MetricUnit::kKilobitsPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_KILOBITS_PER_SECOND},
        {MetricUnit::kMegabitsPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_MEGABITS_PER_SECOND},
        {MetricUnit::kGigabitsPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_GIGABITS_PER_SECOND},
        {MetricUnit::kTerabitsPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_TERABITS_PER_SECOND},
        {MetricUnit::kBytesPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_BYTES_PER_SECOND},
        {MetricUnit::kKilobytesPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_KILOBYTES_PER_SECOND},
        {MetricUnit::kMegabytesPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_MEGABYTES_PER_SECOND},
        {MetricUnit::kGigabytesPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_GIGABYTES_PER_SECOND},
        {MetricUnit::kTerabytesPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_TERABYTES_PER_SECOND},
        {MetricUnit::kCountPerSecond,
         google::cmrt::sdk::metric_service::v1::MetricUnit::
             METRIC_UNIT_COUNT_PER_SECOND}};

namespace google::scp::cpio::client_providers {
cmrt::sdk::metric_service::v1::MetricUnit
MetricClientUtils::ConvertToMetricUnitProto(MetricUnit metric_unit) {
  if (kMetricUnitMap.find(metric_unit) != kMetricUnitMap.end()) {
    return kMetricUnitMap.at(metric_unit);
  }
  return cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_UNKNOWN;
}

ExecutionResult MetricClientUtils::ValidateRequest(
    const PutMetricsRequest& request,
    const shared_ptr<MetricBatchingOptions>& options) {
  // If batching recording is not enabled, the namespace should be set in the
  // request.
  if (!options->enable_batch_recording && request.metric_namespace().empty()) {
    return FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET);
  }

  if (request.metrics().empty()) {
    return FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET);
  }
  for (auto metric : request.metrics()) {
    if (metric.name().empty()) {
      return FailureExecutionResult(
          SC_METRIC_CLIENT_PROVIDER_METRIC_NAME_NOT_SET);
    }
    if (metric.value().empty()) {
      return FailureExecutionResult(
          SC_METRIC_CLIENT_PROVIDER_METRIC_VALUE_NOT_SET);
    }
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio::client_providers
