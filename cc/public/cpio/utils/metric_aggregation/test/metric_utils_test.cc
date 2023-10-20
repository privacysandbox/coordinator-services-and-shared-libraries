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

#include "public/cpio/utils/metric_aggregation/src/metric_utils.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using std::make_shared;
using std::string;

namespace google::scp::cpio {
TEST(MetricUtilsTest, OverrideMetricName) {
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto metric_value = make_shared<MetricValue>("1234");

  auto metric_tag = make_shared<MetricTag>();
  auto update_name = make_shared<MetricName>("ABCDEFG");
  metric_tag->update_name = update_name;
  MetricLabels time_additional_labels;
  time_additional_labels["Type"] = "AverageExecutionTime";
  metric_tag->additional_labels =
      make_shared<MetricLabels>(time_additional_labels);

  auto record_metric_request = make_shared<PutMetricsRequest>();
  MetricUtils::GetPutMetricsRequest(record_metric_request, metric_info,
                                    metric_value, metric_tag);

  EXPECT_EQ(record_metric_request->metrics()[0].name(), *update_name);
  EXPECT_EQ(record_metric_request->metrics()[0].unit(),
            cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_COUNT);
  EXPECT_EQ(record_metric_request->metrics()[0].value(), *metric_value);
}

TEST(MetricUtilsTest, OverrideMetricUnit) {
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto metric_value = make_shared<MetricValue>("1234");

  auto metric_tag = make_shared<MetricTag>();
  auto update_unit = make_shared<MetricUnit>(MetricUnit::kMilliseconds);
  metric_tag->update_unit = update_unit;
  MetricLabels time_additional_labels;
  time_additional_labels["Type"] = "AverageExecutionTime";
  metric_tag->additional_labels =
      make_shared<MetricLabels>(time_additional_labels);

  auto record_metric_request = make_shared<PutMetricsRequest>();
  MetricUtils::GetPutMetricsRequest(record_metric_request, metric_info,
                                    metric_value, metric_tag);

  EXPECT_EQ(record_metric_request->metrics()[0].name(), *metric_name);
  EXPECT_EQ(
      record_metric_request->metrics()[0].unit(),
      cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_MILLISECONDS);
  EXPECT_EQ(record_metric_request->metrics()[0].value(), *metric_value);
}

TEST(MetricUtilsTest, CombineMetricLabelsTagLabels) {
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");

  MetricLabels metric_labels;
  metric_labels["Phase"] = "TestTest";
  metric_info->labels = make_shared<MetricLabels>(metric_labels);

  auto metric_value = make_shared<MetricValue>("1234");

  auto metric_tag = make_shared<MetricTag>();
  auto update_unit = make_shared<MetricUnit>(MetricUnit::kMilliseconds);
  metric_tag->update_unit = update_unit;
  MetricLabels time_additional_labels;
  time_additional_labels["Type"] = "AverageExecutionTime";
  metric_tag->additional_labels =
      make_shared<MetricLabels>(time_additional_labels);

  auto record_metric_request = make_shared<PutMetricsRequest>();
  MetricUtils::GetPutMetricsRequest(record_metric_request, metric_info,
                                    metric_value, metric_tag);

  EXPECT_EQ(record_metric_request->metrics()[0].name(), *metric_name);
  EXPECT_EQ(
      record_metric_request->metrics()[0].unit(),
      cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_MILLISECONDS);
  EXPECT_EQ(record_metric_request->metrics()[0].value(), *metric_value);
  EXPECT_TRUE(record_metric_request->metrics()[0]
                  .labels()
                  .find(string("Type"))
                  ->second == string("AverageExecutionTime"));
  EXPECT_TRUE(record_metric_request->metrics()[0]
                  .labels()
                  .find(string("Phase"))
                  ->second == string("TestTest"));
}

}  // namespace google::scp::cpio
