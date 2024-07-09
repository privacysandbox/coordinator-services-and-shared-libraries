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

#include <thread>
#include <variant>

#include "core/config_provider/mock/mock_config_provider.h"
#include "core/telemetry/mock/in_memory_metric_exporter.h"
#include "core/telemetry/mock/in_memory_metric_router.h"
#include "core/telemetry/src/common/telemetry_configuration.h"
#include "core/telemetry/src/metric/metric_router.h"
#include "include/gtest/gtest.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"

namespace google::scp::core {
namespace {

class MetricRouterInMemoryIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    metric_router_ = std::make_unique<InMemoryMetricRouter>();
  }

  void TearDown() override {
    std::shared_ptr<opentelemetry::metrics::MeterProvider> noop_meter_provider =
        std::make_shared<opentelemetry::metrics::NoopMeterProvider>();
    opentelemetry::metrics::Provider::SetMeterProvider(noop_meter_provider);
  }

  std::unique_ptr<InMemoryMetricRouter> metric_router_;
};

TEST_F(MetricRouterInMemoryIntegrationTest, SuccesfulInitialization) {
  ASSERT_NE(nullptr, &metric_router_->GetMetricExporter());
  ASSERT_NE(nullptr, opentelemetry::metrics::Provider::GetMeterProvider());
  ASSERT_NE(nullptr, &metric_router_->GetMetricReader());
}

/*
* This test replicates the exact behaviour of routing and exporting the data.
*
* Sample exported data
* {
scope name	: test_meter
schema url	: dummy_schema_url
version	        : 1
start time	: Wed Feb 28 01:25:05 2024
end time	: Wed Feb 28 01:25:07 2024
instrument name	: test_counter
description	: test_counter_description
unit		:
type		: SumPointData
value		: 30
attributes	:
  attribute1: value1
  attribute2: 42
resources	:
  service.name: unknown_service
  telemetry.sdk.language: cpp
  telemetry.sdk.name: opentelemetry
  telemetry.sdk.version: 1.13.0
}
*/
TEST_F(MetricRouterInMemoryIntegrationTest,
       ValidateExportingDataUsingGlobalMeterProvider) {
  // getting meter provider globally
  std::shared_ptr<opentelemetry::metrics::MeterProvider> meter_provider =
      opentelemetry::metrics::Provider::GetMeterProvider();

  std::shared_ptr<opentelemetry::metrics::Meter> meter =
      meter_provider->GetMeter("test_meter", "1", "dummy_schema_url");
  std::unique_ptr<opentelemetry::metrics::Counter<double>> counter =
      meter->CreateDoubleCounter("test_counter", "test_counter_description");

  std::map<std::string, opentelemetry::common::AttributeValue> attributes_map =
      {{"attribute1", "value1"}, {"attribute2", 42}};
  counter->Add(10, attributes_map);
  counter->Add(20, attributes_map);
  // it would be a different point data attribute if we don't add the same
  // attributes here

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  opentelemetry::sdk::metrics::ResourceMetrics resource_metrics = data[0];

  std::vector<opentelemetry::sdk::metrics::ScopeMetrics> scope_metric_data =
      resource_metrics.scope_metric_data_;
  ASSERT_EQ(scope_metric_data.size(), 1);

  opentelemetry::sdk::metrics::ScopeMetrics scope_metrics =
      scope_metric_data[0];
  const opentelemetry::sdk::instrumentationscope::InstrumentationScope* scope =
      scope_metrics.scope_;

  ASSERT_EQ(scope->GetName(), "test_meter");
  ASSERT_EQ(scope->GetVersion(), "1");
  ASSERT_EQ(scope->GetSchemaURL(), "dummy_schema_url");

  ASSERT_EQ(scope->GetAttributes(), opentelemetry::sdk::common::AttributeMap());

  std::vector<opentelemetry::sdk::metrics::MetricData> metric_data =
      scope_metrics.metric_data_;
  ASSERT_EQ(metric_data.size(), 1);

  opentelemetry::sdk::metrics::MetricData metric = metric_data[0];
  opentelemetry::sdk::metrics::AggregationTemporality aggregation_temporality =
      metric.aggregation_temporality;
  ASSERT_EQ(aggregation_temporality,
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);

  opentelemetry::sdk::metrics::InstrumentDescriptor instrument_descriptor =
      metric.instrument_descriptor;
  ASSERT_EQ(instrument_descriptor.name_, "test_counter");
  ASSERT_EQ(instrument_descriptor.description_, "test_counter_description");
  ASSERT_EQ(instrument_descriptor.type_,
            opentelemetry::sdk::metrics::InstrumentType::kCounter);
  ASSERT_EQ(instrument_descriptor.unit_, "");
  ASSERT_EQ(instrument_descriptor.value_type_,
            opentelemetry::sdk::metrics::InstrumentValueType::kDouble);

  std::vector<opentelemetry::sdk::metrics::PointDataAttributes>
      point_data_attrs = metric.point_data_attr_;
  ASSERT_EQ(point_data_attrs.size(), 1);

  opentelemetry::sdk::metrics::PointDataAttributes point_data_attr =
      point_data_attrs[0];
  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      point_data_attr.point_data));

  auto sum_point_data = std::get<opentelemetry::sdk::metrics::SumPointData>(
      point_data_attr.point_data);

  ASSERT_EQ(std::get<double>(sum_point_data.value_), 30);

  opentelemetry::sdk::common::OrderedAttributeMap dimensions =
      point_data_attr.attributes;
  ASSERT_EQ(dimensions.size(), 2);
  ASSERT_EQ(dimensions, opentelemetry::sdk::common::OrderedAttributeMap(
                            {{"attribute1", "value1"}, {"attribute2", 42}}));
}
}  // namespace
}  // namespace google::scp::core
