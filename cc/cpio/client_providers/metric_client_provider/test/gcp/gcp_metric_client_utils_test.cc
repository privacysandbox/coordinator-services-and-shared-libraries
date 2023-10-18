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

#include "cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_utils.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>

#include <google/protobuf/util/time_util.h>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/metric_client_provider/src/gcp/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::api::MonitoredResource;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::monitoring::v3::Point;
using google::monitoring::v3::TimeSeries;
using google::protobuf::MapPair;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_FAILED_OVERSIZE_METRIC_LABELS;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP;
using google::scp::core::errors::SC_GCP_METRIC_CLIENT_INVALID_METRIC_VALUE;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::GcpMetricClientUtils;
using std::make_shared;
using std::map;
using std::stod;
using std::string;
using std::to_string;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

namespace google::scp::cpio::test {
static constexpr char kName[] = "test_name";
static constexpr char kValue[] = "12346.89";
static constexpr char kBadValue[] = "ab33c6";
static constexpr MetricUnit kUnit = MetricUnit::METRIC_UNIT_COUNT;
static constexpr char kNamespace[] = "test_namespace";
static constexpr char kMetricTypePrefix[] = "custom.googleapis.com";
static constexpr char kProjectIdValue[] = "project_id_test";
static constexpr char kInstanceIdValue[] = "instance_id_test";
static constexpr char kInstanceZoneValue[] = "zone_test";
static constexpr char kResourceType[] = "gce_instance";
static constexpr char kProjectIdKey[] = "project_id";
static constexpr char kInstanceIdKey[] = "instance_id";
static constexpr char kInstanceZoneKey[] = "zone";

class GcpMetricClientUtilsTest : public ::testing::Test {
 protected:
  void SetPutMetricsRequest(
      PutMetricsRequest& record_metric_request, const string& value = kValue,
      const int64_t& timestamp_in_ms =
          duration_cast<milliseconds>(system_clock::now().time_since_epoch())
              .count()) {
    auto metric = record_metric_request.add_metrics();
    metric->set_name(kName);
    metric->set_value(value);
    metric->set_unit(kUnit);
    *metric->mutable_timestamp() =
        TimeUtil::MillisecondsToTimestamp(timestamp_in_ms);
    map<string, string> labels{
        {"CPU", "10"},
        {"GPU", "15"},
        {"RAM", "20"},
    };
    metric->mutable_labels()->insert(labels.begin(), labels.end());
  }
};

TEST_F(GcpMetricClientUtilsTest, ParseRequestToTimeSeries) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  vector<TimeSeries> time_series_list;

  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  auto expected_type =
      string(kMetricTypePrefix) + "/" + string(kNamespace) + "/test_name";
  auto expected_timestamp = context.request->metrics()[0].timestamp();

  auto time_series = time_series_list[0];
  EXPECT_SUCCESS(result);
  EXPECT_EQ(time_series.metric().type(), expected_type);
  EXPECT_EQ(time_series.unit(), "");
  EXPECT_EQ(time_series.metric().labels().size(), 3);
  EXPECT_EQ(time_series.points()[0].value().double_value(), stod(kValue));
  EXPECT_EQ(time_series.points()[0].interval().end_time(), expected_timestamp);
}

TEST_F(GcpMetricClientUtilsTest, FailedWithBadMetricValue) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kBadValue);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  vector<TimeSeries> time_series_list;

  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  auto expected_type =
      string(kMetricTypePrefix) + "/" + string(kNamespace) + "/test_name";
  auto expected_timestamp = context.request->metrics()[0].timestamp();

  auto time_series = time_series_list[0];
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_GCP_METRIC_CLIENT_INVALID_METRIC_VALUE)));
}

TEST_F(GcpMetricClientUtilsTest, BadTimeStamp) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kValue, -123);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  vector<TimeSeries> time_series_list;

  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP)));
}

TEST_F(GcpMetricClientUtilsTest, InvalidTimeStamp) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kValue, 12345);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  vector<TimeSeries> time_series_list;

  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP)));
}

TEST_F(GcpMetricClientUtilsTest, OverSizeLabels) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  // Adds oversize labels.
  auto labels = context.request->add_metrics()->mutable_labels();
  for (int i = 0; i < 33; ++i) {
    labels->insert(MapPair(string("key") + to_string(i), string("value")));
  }
  vector<TimeSeries> time_series_list;
  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_GCP_METRIC_CLIENT_FAILED_OVERSIZE_METRIC_LABELS)));
}

TEST(GcpMetricClientUtilsTestII, AddResourceToTimeSeries) {
  vector<TimeSeries> time_series_list(10);

  GcpMetricClientUtils::AddResourceToTimeSeries(
      kProjectIdValue, kInstanceIdValue, kInstanceZoneValue, time_series_list);

  for (auto time_series : time_series_list) {
    auto resouce = time_series.resource();
    EXPECT_EQ(resouce.type(), kResourceType);
    EXPECT_EQ(resouce.labels().find(kProjectIdKey)->second, kProjectIdValue);
    EXPECT_EQ(resouce.labels().find(kInstanceIdKey)->second, kInstanceIdValue);
    EXPECT_EQ(resouce.labels().find(kInstanceZoneKey)->second,
              kInstanceZoneValue);
  }
}

}  // namespace google::scp::cpio::test
