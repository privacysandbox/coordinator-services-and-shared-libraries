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

#include "cpio/client_providers/metric_client_provider/src/aws/aws_metric_client_utils.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <aws/monitoring/CloudWatchErrors.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <google/protobuf/util/time_util.h>

#include "core/interface/async_context.h"
#include "cpio/client_providers/metric_client_provider/src/aws/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using Aws::Client::AWSError;
using Aws::CloudWatch::CloudWatchErrors;
using Aws::CloudWatch::Model::Dimension;
using Aws::CloudWatch::Model::MetricDatum;
using Aws::CloudWatch::Model::PutMetricDataOutcome;
using Aws::CloudWatch::Model::PutMetricDataRequest;
using Aws::CloudWatch::Model::StandardUnitMapper::GetStandardUnitForName;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::MapPair;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
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
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::AwsMetricClientUtils;
using std::make_shared;
using std::map;
using std::stod;
using std::string;
using std::to_string;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::milliseconds;
using std::chrono::system_clock;

namespace google::scp::cpio::client_providers::test {
static constexpr size_t kAwsMetricDatumSizeLimit = 1000;
constexpr char kName[] = "test_name";
constexpr char kValue[] = "12346";
const MetricUnit kUnit = MetricUnit::METRIC_UNIT_COUNT;

class AwsMetricClientUtilsTest : public ::testing::Test {
 protected:
  void SetPutMetricsRequest(
      PutMetricsRequest& record_metric_request, const string& value = kValue,
      int metrics_num = 1,
      int64_t timestamp_in_ms =
          duration_cast<milliseconds>(system_clock::now().time_since_epoch())
              .count()) {
    for (auto i = 0; i < metrics_num; i++) {
      auto metric = record_metric_request.add_metrics();
      metric->set_name(kName);
      metric->set_value(value);
      metric->set_unit(kUnit);
      *metric->mutable_timestamp() =
          TimeUtil::MillisecondsToTimestamp(timestamp_in_ms);
    }
  }
};

TEST_F(AwsMetricClientUtilsTest, ParseRequestToDatumSuccess) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kValue,
                       /*Generates 10 metrics in one request */ 10);

  bool parse_request_to_datum_is_called = false;
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        parse_request_to_datum_is_called = true;
      });
  vector<MetricDatum> datum_list;
  EXPECT_SUCCESS(AwsMetricClientUtils::ParseRequestToDatum(
      context, datum_list, kAwsMetricDatumSizeLimit));
  EXPECT_TRUE(!parse_request_to_datum_is_called);
  EXPECT_EQ(datum_list.size(), 10);
  for (auto i = 0; i < 10; i++) {
    EXPECT_EQ(datum_list.at(i).GetMetricName(), kName);
    EXPECT_EQ(datum_list.at(i).GetValue(), stod(kValue));
    EXPECT_EQ(datum_list.at(i).GetUnit(),
              Aws::CloudWatch::Model::StandardUnit::Count);
  }
}

TEST_F(AwsMetricClientUtilsTest, OversizeMetricsInRequest) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kValue, 1001);

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
  vector<MetricDatum> datum_list;
  EXPECT_THAT(
      AwsMetricClientUtils::ParseRequestToDatum(context, datum_list,
                                                kAwsMetricDatumSizeLimit),
      ResultIs(FailureExecutionResult(
          SC_AWS_METRIC_CLIENT_PROVIDER_METRIC_LIMIT_REACHED_PER_REQUEST)));
  EXPECT_EQ(datum_list.size(), 0);
}

TEST_F(AwsMetricClientUtilsTest, ParseRequestToDatumInvalidValue) {
  PutMetricsRequest record_metric_request;
  auto invalid_value = "abcd";
  SetPutMetricsRequest(record_metric_request, invalid_value);
  bool parse_request_to_datum_is_called = false;
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        parse_request_to_datum_is_called = true;
      });
  vector<MetricDatum> datum_list;
  EXPECT_THAT(AwsMetricClientUtils::ParseRequestToDatum(
                  context, datum_list, kAwsMetricDatumSizeLimit),
              ResultIs(FailureExecutionResult(
                  SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_VALUE)));
  EXPECT_TRUE(datum_list.empty());
  EXPECT_TRUE(parse_request_to_datum_is_called);
}

TEST_F(AwsMetricClientUtilsTest, ParseRequestToDatumInvalidTimestamp) {
  PutMetricsRequest record_metric_request;
  Timestamp negative_time = -1234;
  auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
  Timestamp old_time_stamp =
      current_time - duration_cast<milliseconds>(hours(24 * 15)).count();
  Timestamp ahead_time_stamp =
      current_time + duration_cast<milliseconds>(hours(24 * 15)).count();
  vector<Timestamp> timestamp_vector = {negative_time, old_time_stamp,
                                        ahead_time_stamp};

  for (const auto& timestamp : timestamp_vector) {
    SetPutMetricsRequest(record_metric_request, kValue, 1, timestamp);
    bool parse_request_to_datum_is_called = false;
    AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
        make_shared<PutMetricsRequest>(record_metric_request),
        [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
          parse_request_to_datum_is_called = true;
        });
    vector<MetricDatum> datum_list;
    EXPECT_THAT(AwsMetricClientUtils::ParseRequestToDatum(
                    context, datum_list, kAwsMetricDatumSizeLimit),
                ResultIs(FailureExecutionResult(
                    SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_TIMESTAMP)));
    EXPECT_TRUE(datum_list.empty());
    EXPECT_TRUE(parse_request_to_datum_is_called);
  }
}

TEST_F(AwsMetricClientUtilsTest, ParseRequestToDatumOversizeDimensions) {
  PutMetricsRequest record_metric_request;
  auto metric = record_metric_request.add_metrics();
  metric->set_name(kName);
  metric->set_value(kValue);
  metric->set_unit(kUnit);

  auto metric_labels_ = metric->mutable_labels();
  string label_value = "test";
  for (auto i = 0; i < 31; i++) {
    metric_labels_->insert(MapPair(to_string(i), label_value));
  }

  bool parse_request_to_datum_is_called = false;
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        parse_request_to_datum_is_called = true;
      });
  vector<MetricDatum> datum_list;
  EXPECT_THAT(AwsMetricClientUtils::ParseRequestToDatum(
                  context, datum_list, kAwsMetricDatumSizeLimit),
              ResultIs(FailureExecutionResult(
                  SC_AWS_METRIC_CLIENT_PROVIDER_OVERSIZE_DATUM_DIMENSIONS)));
  EXPECT_TRUE(datum_list.empty());
  EXPECT_TRUE(parse_request_to_datum_is_called);
}

TEST_F(AwsMetricClientUtilsTest, ParseRequestToDatumInvalidUnit) {
  PutMetricsRequest record_metric_request;
  auto metric = record_metric_request.add_metrics();
  metric->set_name(kName);
  metric->set_value(kValue);
  metric->set_unit(MetricUnit::METRIC_UNIT_UNKNOWN);

  bool parse_request_to_datum_is_called = false;
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        parse_request_to_datum_is_called = true;
      });
  vector<MetricDatum> datum_list;
  auto result = AwsMetricClientUtils::ParseRequestToDatum(
      context, datum_list, kAwsMetricDatumSizeLimit);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_AWS_METRIC_CLIENT_PROVIDER_INVALID_METRIC_UNIT)));
  EXPECT_TRUE(datum_list.empty());
  EXPECT_TRUE(parse_request_to_datum_is_called);
}

}  // namespace google::scp::cpio::client_providers::test
