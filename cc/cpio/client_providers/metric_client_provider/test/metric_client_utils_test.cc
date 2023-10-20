
// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpio/client_providers/metric_client_provider/src/metric_client_utils.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "cpio/client_providers/metric_client_provider/src/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NAME_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_VALUE_NOT_SET;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::MetricClientUtils;
using std::make_shared;

static constexpr char kMetricNamespace[] = "namespace";

namespace google::scp::cpio::client_providers::test {
TEST(MetricClientUtilsTest, ConvertMetricUnit) {
  EXPECT_EQ(MetricClientUtils::ConvertToMetricUnitProto(MetricUnit::kBits),
            cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_BITS);
  EXPECT_EQ(MetricClientUtils::ConvertToMetricUnitProto(MetricUnit::kCount),
            cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_COUNT);
  EXPECT_EQ(
      MetricClientUtils::ConvertToMetricUnitProto(MetricUnit::kCountPerSecond),
      cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_COUNT_PER_SECOND);
}

TEST(MetricClientUtilsTest, NoMetric) {
  PutMetricsRequest request;
  request.set_metric_namespace(kMetricNamespace);
  EXPECT_THAT(MetricClientUtils::ValidateRequest(
                  request, make_shared<MetricBatchingOptions>()),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET)));
}

TEST(MetricClientUtilsTest, NoMetricName) {
  PutMetricsRequest request;
  request.set_metric_namespace(kMetricNamespace);
  request.add_metrics();

  EXPECT_THAT(MetricClientUtils::ValidateRequest(
                  request, make_shared<MetricBatchingOptions>()),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_METRIC_NAME_NOT_SET)));
}

TEST(MetricClientUtilsTest, NoMetricValue) {
  PutMetricsRequest request;
  request.set_metric_namespace(kMetricNamespace);
  auto metric = request.add_metrics();
  metric->set_name("metric1");
  EXPECT_THAT(MetricClientUtils::ValidateRequest(
                  request, make_shared<MetricBatchingOptions>()),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_METRIC_VALUE_NOT_SET)));
}

TEST(MetricClientUtilsTest, OneMetricWithoutName) {
  PutMetricsRequest request;
  request.set_metric_namespace(kMetricNamespace);
  auto metric = request.add_metrics();
  metric->set_name("metric1");
  metric->set_value("123");
  request.add_metrics();

  EXPECT_THAT(MetricClientUtils::ValidateRequest(
                  request, make_shared<MetricBatchingOptions>()),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_METRIC_NAME_NOT_SET)));
}

TEST(MetricClientUtilsTest, NoNamespaceWhenOptionsAreSet) {
  PutMetricsRequest request;
  auto metric = request.add_metrics();
  metric->set_name("metric1");
  metric->set_value("123");
  auto options = make_shared<MetricBatchingOptions>();
  options->enable_batch_recording = true;
  EXPECT_SUCCESS(MetricClientUtils::ValidateRequest(request, options));
}

TEST(MetricClientUtilsTest, ValidMetric) {
  PutMetricsRequest request;
  request.set_metric_namespace(kMetricNamespace);
  auto metric = request.add_metrics();
  metric->set_name("metric1");
  metric->set_value("123");
  EXPECT_SUCCESS(MetricClientUtils::ValidateRequest(
      request, make_shared<MetricBatchingOptions>()));
}
}  // namespace google::scp::cpio::client_providers::test
