//  Copyright 2024 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "core/telemetry/src/common/metric_utils.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "include/gtest/gtest.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace google::scp::core {

namespace {

// ResourceMetricsInitializer
//  └─ std::vector<InstrumentAndPoints>
//                  ├─ InstrumentName
//                  └─ std::vector<ValueAndAttributes>
//                                  ├─ PointValue
//                                  └─ PointAttributes
using ResourceMetricsInitializer = std::vector<std::pair<
    std::string, std::vector<std::pair<
                     double, opentelemetry::sdk::metrics::PointAttributes>>>>;

// Create a ResourceMetrics using a simplified initializer format.
opentelemetry::sdk::metrics::ResourceMetrics CreateResourceMetrics(
    const ResourceMetricsInitializer& resource_metrics_initializer) {
  std::vector<opentelemetry::sdk::metrics::MetricData> metric_data_vector;
  for (const auto& instrument_and_points : resource_metrics_initializer) {
    const auto& instrument_name = instrument_and_points.first;
    const auto& value_and_attributes_vector = instrument_and_points.second;

    std::vector<opentelemetry::sdk::metrics::PointDataAttributes> points;
    for (const auto& value_and_attributes : value_and_attributes_vector) {
      const auto& value = value_and_attributes.first;
      const auto& attributes = value_and_attributes.second;

      opentelemetry::sdk::metrics::LastValuePointData point_type;
      point_type.value_ = value;
      opentelemetry::sdk::metrics::PointDataAttributes point{
          .attributes = attributes, .point_data = point_type};
      points.push_back(point);
    }

    opentelemetry::sdk::metrics::InstrumentDescriptor instrument_descriptor{
        .name_ = instrument_name};
    opentelemetry::sdk::metrics::MetricData metric_data{
        .instrument_descriptor = instrument_descriptor,
        .point_data_attr_ = points};
    metric_data_vector.push_back(metric_data);
  }

  opentelemetry::sdk::metrics::ScopeMetrics scope_metrics(/*scope=*/nullptr,
                                                          metric_data_vector);
  opentelemetry::sdk::metrics::ResourceMetrics resource_metrics(
      /*resource=*/nullptr, scope_metrics);
  return resource_metrics;
}

TEST(SimpleTest, GetMetricPointDataInstrumentNameTest) {
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
      resource_metrics_vector = {CreateResourceMetrics(
          {{"instrument_name_1",
            {{1.1, {{"p1.1_k1", "p1.1_v1"}, {"p1.1_k2", "p1.1_v2"}}},
             {1.2, {{"p1.2_k1", "p1.2_v1"}, {"p1.2_k2", "p1.2_v2"}}}}}})};

  auto point_type_opt =
      GetMetricPointData("instrument_name_1", {}, resource_metrics_vector);
  ASSERT_TRUE(point_type_opt.has_value());
  auto point_type =
      std::move(std::get<opentelemetry::sdk::metrics::LastValuePointData>(
          point_type_opt.value()));
  EXPECT_EQ(std::get<double>(point_type.value_), 1.1);

  point_type_opt =
      GetMetricPointData("instrument_name_3", {}, resource_metrics_vector);
  EXPECT_FALSE(point_type_opt.has_value());
}

TEST(SimpleTest, GetMetricPointDataMatchTest) {
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
      resource_metrics_vector = {CreateResourceMetrics(
          {{"instrument_name_1",
            {{1.1, {{"p1.1_k1", "p1.1_v1"}, {"p1.1_k2", "p1.1_v2"}}},
             {1.2, {{"p1.2_k1", "p1.2_v1"}, {"p1.2_k2", "p1.2_v2"}}}}},
           {"instrument_name_2",
            {{2.1, {{"p2.1_k1", "p2.1_v1"}, {"p2.1_k2", "p2.1_v2"}}},
             {2.2, {{"p2.2_k1", "p2.2_v1"}, {"p2.2_k2", "p2.2_v2"}}}}}})};

  auto point_type_opt = GetMetricPointData(
      "instrument_name_2", {{"p2.2_k1", "p2.2_v1"}}, resource_metrics_vector);
  ASSERT_TRUE(point_type_opt.has_value());
  auto point_type =
      std::move(std::get<opentelemetry::sdk::metrics::LastValuePointData>(
          point_type_opt.value()));
  EXPECT_EQ(std::get<double>(point_type.value_), 2.2);

  point_type_opt = GetMetricPointData(
      "instrument_name_1", {{"p1.2_k1", "p1.2_v1"}, {"p1.2_k2", "p1.2_v2"}},
      resource_metrics_vector);
  ASSERT_TRUE(point_type_opt.has_value());
  point_type =
      std::move(std::get<opentelemetry::sdk::metrics::LastValuePointData>(
          point_type_opt.value()));
  EXPECT_EQ(std::get<double>(point_type.value_), 1.2);
}

TEST(SimpleTest, GetMetricPointDataMismatchTest) {
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
      resource_metrics_vector = {CreateResourceMetrics(
          {{"instrument_name_1",
            {{1.1, {{"p1.1_k1", "p1.1_v1"}, {"p1.1_k2", "p1.1_v2"}}},
             {1.2, {{"p1.2_k1", "p1.2_v1"}, {"p1.2_k2", "p1.2_v2"}}}}},
           {"instrument_name_2",
            {{2.1, {{"p2.1_k1", "p2.1_v1"}, {"p2.1_k2", "p2.1_v2"}}},
             {2.2, {{"p2.2_k1", "p2.2_v1"}, {"p2.2_k2", "p2.2_v2"}}}}}})};

  auto point_type_opt = GetMetricPointData(
      "instrument_name_1",
      {{"p1.1_k1", "p1.1_v1"}, {"p1.1_k2", "p1.1_v2"}, {"p2.1_k1", "p2.1_v1"}},
      resource_metrics_vector);
  EXPECT_FALSE(point_type_opt.has_value());

  point_type_opt = GetMetricPointData(
      "instrument_name_2", {{"p2.1_k1", "p2.1_v1"}, {"p2.2_k2", "p2.2_v2"}},
      resource_metrics_vector);
  EXPECT_FALSE(point_type_opt.has_value());
}

TEST(SimpleTest, GetMetricAttributesMatchTest) {
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
      resource_metrics_vector = {CreateResourceMetrics(
          {{"instrument_name_1",
            {{1.1, {{"p1.1_k1", "p1.1_v1"}, {"p1.1_k2", "p1.1_v2"}}},
             {1.2, {{"p1.2_k1", "p1.2_v1"}, {"p1.2_k2", "p1.2_v2"}}}}},
           {"instrument_name_2",
            {{2.1, {{"p2.1_k1", "p2.1_v1"}, {"p2.1_k2", "p2.1_v2"}}},
             {2.2, {{"p2.2_k1", "p2.2_v1"}, {"p2.2_k2", "p2.2_v2"}}}}}})};

  auto metric_labels_opt =
      GetMetricAttributes("instrument_name_1", resource_metrics_vector);
  ASSERT_TRUE(metric_labels_opt.has_value());
  opentelemetry::sdk::common::OrderedAttributeMap attributes = {
      {"p1.1_k1", "p1.1_v1"}, {"p1.1_k2", "p1.1_v2"}};
  EXPECT_EQ(metric_labels_opt.value(), attributes);

  metric_labels_opt =
      GetMetricAttributes("instrument_name_2", resource_metrics_vector);
  ASSERT_TRUE(metric_labels_opt.has_value());
  attributes = {{"p2.1_k1", "p2.1_v1"}, {"p2.1_k2", "p2.1_v2"}};
  EXPECT_EQ(metric_labels_opt.value(), attributes);
}

TEST(SimpleTest, GetMetricAttributesMismatchTest) {
  std::vector<opentelemetry::sdk::metrics::ResourceMetrics>
      resource_metrics_vector = {CreateResourceMetrics(
          {{"instrument_name_1",
            {{1.1, {{"p1.1_k1", "p1.1_v1"}, {"p1.1_k2", "p1.1_v2"}}},
             {1.2, {{"p1.2_k1", "p1.2_v1"}, {"p1.2_k2", "p1.2_v2"}}}}}})};

  auto metric_labels_opt =
      GetMetricAttributes("instrument_name_2", resource_metrics_vector);
  EXPECT_FALSE(metric_labels_opt.has_value());

  metric_labels_opt = GetMetricAttributes("", resource_metrics_vector);
  EXPECT_FALSE(metric_labels_opt.has_value());
}

}  // namespace

}  // namespace google::scp::core
