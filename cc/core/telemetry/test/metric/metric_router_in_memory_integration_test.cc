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
    data_ready_ = false;

    exporter_ = std::make_unique<InMemoryMetricExporter>(data_ready_);
    mock_config_provider_ =
        std::make_shared<config_provider::mock::MockConfigProvider>();

    std::int32_t metric_export_interval = 1000;
    std::int32_t metric_export_timeout = 500;
    mock_config_provider_->SetInt32(
        std::string(kOtelMetricExportIntervalMsecKey),
        metric_export_interval);  // exporting every 1s
    mock_config_provider_->SetInt32(
        std::string(kOtelMetricExportTimeoutMsecKey),
        metric_export_timeout);  // timeout every .5s
    metric_router_ = std::make_unique<MetricRouter>(
        MetricRouter(mock_config_provider_, std::move(exporter_)));
  }

  void TearDown() override {
    std::shared_ptr<opentelemetry::metrics::MeterProvider> none;
    opentelemetry::metrics::Provider::SetMeterProvider(none);
  }

  std::shared_ptr<config_provider::mock::MockConfigProvider>
      mock_config_provider_;
  std::unique_ptr<MetricRouter> metric_router_;
  std::atomic<bool> data_ready_;
  std::unique_ptr<InMemoryMetricExporter> exporter_;
};

TEST_F(MetricRouterInMemoryIntegrationTest, SuccesfulInitialization) {
  ASSERT_NE(nullptr, &metric_router_->exporter());
  ASSERT_NE(nullptr, metric_router_->meter_provider());
}

TEST_F(MetricRouterInMemoryIntegrationTest, ValidateExporterInMetricRouter) {
  auto& base_exporter = metric_router_->exporter();

  auto* in_memory_exporter =
      dynamic_cast<InMemoryMetricExporter*>(&base_exporter);
  ASSERT_TRUE(in_memory_exporter != nullptr);
}

TEST_F(MetricRouterInMemoryIntegrationTest,
       ValidateMeterProviderInMetricRouter) {
  auto meter_provider = metric_router_->meter_provider();
  ASSERT_TRUE(meter_provider != nullptr);
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
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
      meter_provider = opentelemetry::metrics::Provider::GetMeterProvider();

  // this works too (using metric router)
  // opentelemetry::metric::MeterProvider& meter_provider =
  // metric_router_->meter_provider();

  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter =
      meter_provider->GetMeter("test_meter", "1", "dummy_schema_url");
  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<double>>
      counter = meter->CreateDoubleCounter("test_counter",
                                           "test_counter_description");

  std::map<std::string, opentelemetry::common::AttributeValue> attributes_map =
      {{"attribute1", "value1"}, {"attribute2", 42}};
  counter->Add(10, attributes_map);
  counter->Add(20, attributes_map);
  // it would be a different point data attribute if we don't add the same
  // attributes here

  // active waiting for data to export
  while (!data_ready_.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }

  // Validations
  // getting exporter from metric_router to make sure exporter is set correctly
  auto& base_exporter = metric_router_->exporter();
  auto& in_memory_exporter =
      dynamic_cast<InMemoryMetricExporter&>(base_exporter);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      in_memory_exporter.data();
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
  ASSERT_TRUE(opentelemetry::nostd::holds_alternative<
              opentelemetry::sdk::metrics::SumPointData>(
      point_data_attr.point_data));

  auto sum_point_data =
      opentelemetry::nostd::get<opentelemetry::sdk::metrics::SumPointData>(
          point_data_attr.point_data);

  ASSERT_EQ(opentelemetry::nostd::get<double>(sum_point_data.value_), 30);

  opentelemetry::sdk::common::OrderedAttributeMap dimensions =
      point_data_attr.attributes;
  ASSERT_EQ(dimensions.size(), 2);
  ASSERT_EQ(dimensions, opentelemetry::sdk::common::OrderedAttributeMap(
                            {{"attribute1", "value1"}, {"attribute2", 42}}));
}
}  // namespace
}  // namespace google::scp::core
