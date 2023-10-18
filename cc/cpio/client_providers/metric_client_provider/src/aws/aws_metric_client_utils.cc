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

#include "aws_metric_client_utils.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/CloudWatchErrors.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <google/protobuf/util/time_util.h>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"

using Aws::CloudWatch::Model::Dimension;
using Aws::CloudWatch::Model::MetricDatum;
using Aws::CloudWatch::Model::StandardUnit;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_UNIT;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_VALUE;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_TIMESTAMP;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_METRIC_LIMIT_REACHED_PER_REQUEST;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_OVERSIZE_DATUM_DIMENSIONS;
using std::map;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::seconds;
using std::chrono::system_clock;

static constexpr int kTwoWeeksSecondsCount =
    duration_cast<seconds>(hours(24 * 14)).count();
static constexpr int kTwoHoursSecondsCount =
    duration_cast<seconds>(hours(2)).count();

static const map<MetricUnit, StandardUnit> kAwsMetricUnitMap = {
    {MetricUnit::METRIC_UNIT_UNKNOWN, StandardUnit::NOT_SET},
    {MetricUnit::METRIC_UNIT_SECONDS, StandardUnit::Seconds},
    {MetricUnit::METRIC_UNIT_MICROSECONDS, StandardUnit::Microseconds},
    {MetricUnit::METRIC_UNIT_MILLISECONDS, StandardUnit::Milliseconds},
    {MetricUnit::METRIC_UNIT_BITS, StandardUnit::Bits},
    {MetricUnit::METRIC_UNIT_KILOBITS, StandardUnit::Kilobits},
    {MetricUnit::METRIC_UNIT_MEGABITS, StandardUnit::Megabits},
    {MetricUnit::METRIC_UNIT_GIGABITS, StandardUnit::Gigabits},
    {MetricUnit::METRIC_UNIT_TERABITS, StandardUnit::Terabits},
    {MetricUnit::METRIC_UNIT_BYTES, StandardUnit::Bytes},
    {MetricUnit::METRIC_UNIT_KILOBYTES, StandardUnit::Kilobytes},
    {MetricUnit::METRIC_UNIT_MEGABYTES, StandardUnit::Megabytes},
    {MetricUnit::METRIC_UNIT_GIGABYTES, StandardUnit::Gigabytes},
    {MetricUnit::METRIC_UNIT_TERABYTES, StandardUnit::Terabytes},
    {MetricUnit::METRIC_UNIT_COUNT, StandardUnit::Count},
    {MetricUnit::METRIC_UNIT_PERCENT, StandardUnit::Percent},
    {MetricUnit::METRIC_UNIT_BITS_PER_SECOND, StandardUnit::Bits_Second},
    {MetricUnit::METRIC_UNIT_KILOBITS_PER_SECOND,
     StandardUnit::Kilobits_Second},
    {MetricUnit::METRIC_UNIT_MEGABITS_PER_SECOND,
     StandardUnit::Megabits_Second},
    {MetricUnit::METRIC_UNIT_GIGABITS_PER_SECOND,
     StandardUnit::Gigabits_Second},
    {MetricUnit::METRIC_UNIT_TERABITS_PER_SECOND,
     StandardUnit::Terabits_Second},
    {MetricUnit::METRIC_UNIT_BYTES_PER_SECOND, StandardUnit::Bytes_Second},
    {MetricUnit::METRIC_UNIT_KILOBYTES_PER_SECOND,
     StandardUnit::Kilobytes_Second},
    {MetricUnit::METRIC_UNIT_MEGABYTES_PER_SECOND,
     StandardUnit::Megabytes_Second},
    {MetricUnit::METRIC_UNIT_GIGABYTES_PER_SECOND,
     StandardUnit::Gigabytes_Second},
    {MetricUnit::METRIC_UNIT_TERABYTES_PER_SECOND,
     StandardUnit::Terabytes_Second},
    {MetricUnit::METRIC_UNIT_COUNT_PER_SECOND, StandardUnit::Count_Second}};

namespace google::scp::cpio::client_providers {

ExecutionResult AwsMetricClientUtils::ParseRequestToDatum(
    AsyncContext<PutMetricsRequest, PutMetricsResponse>& record_metric_context,
    vector<MetricDatum>& datum_list, int request_metric_limit) noexcept {
  if (record_metric_context.request->metrics().size() > request_metric_limit) {
    record_metric_context.result = FailureExecutionResult(
        SC_AWS_METRIC_CLIENT_PROVIDER_METRIC_LIMIT_REACHED_PER_REQUEST);
    record_metric_context.Finish();
    return record_metric_context.result;
  }

  for (auto metric : record_metric_context.request->metrics()) {
    if (metric.labels().size() > 30) {
      record_metric_context.result = FailureExecutionResult(
          SC_AWS_METRIC_CLIENT_PROVIDER_OVERSIZE_DATUM_DIMENSIONS);
      record_metric_context.Finish();
      return record_metric_context.result;
    }

    auto input_timestamp_in_ms =
        TimeUtil::TimestampToMilliseconds(metric.timestamp());
    if (input_timestamp_in_ms < 0) {
      record_metric_context.result = FailureExecutionResult(
          SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_TIMESTAMP);
      record_metric_context.Finish();
      return record_metric_context.result;
    }

    MetricDatum datum;
    // The default value of the timestamp is the current time.
    auto metric_timestamp = Aws::Utils::DateTime(system_clock::now());
    if (input_timestamp_in_ms > 0) {
      auto current_time = Aws::Utils::DateTime().Now();
      metric_timestamp = Aws::Utils::DateTime(input_timestamp_in_ms);
      auto difference =
          duration_cast<seconds>(current_time - metric_timestamp).count();
      // The valid timestamp of metric cannot be earlier than two weeks or
      // later than two hours.
      if (difference > kTwoWeeksSecondsCount ||
          difference < -kTwoHoursSecondsCount) {
        record_metric_context.result = FailureExecutionResult(
            SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_TIMESTAMP);
        record_metric_context.Finish();
        return record_metric_context.result;
      }
    }
    datum.SetTimestamp(metric_timestamp);

    datum.SetMetricName(metric.name().c_str());
    try {
      auto value = std::stod(metric.value());
      datum.SetValue(value);
    } catch (...) {
      record_metric_context.result = FailureExecutionResult(
          SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_VALUE);
      record_metric_context.Finish();
      return record_metric_context.result;
    }

    auto unit = StandardUnit::NOT_SET;
    if (kAwsMetricUnitMap.find(metric.unit()) != kAwsMetricUnitMap.end()) {
      unit = kAwsMetricUnitMap.at(metric.unit());
    }
    if (unit == StandardUnit::NOT_SET) {
      record_metric_context.result = FailureExecutionResult(
          SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_UNIT);
      record_metric_context.Finish();
      return record_metric_context.result;
    }
    datum.SetUnit(unit);

    for (const auto& label : metric.labels()) {
      Dimension dimension;
      dimension.SetName(label.first.c_str());
      dimension.SetValue(label.second.c_str());
      datum.AddDimensions(dimension);
    }

    datum_list.emplace_back(datum);
  }

  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio::client_providers
