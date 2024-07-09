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

#include "core/telemetry/mock/in_memory_metric_exporter.h"

#include "include/gtest/gtest.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace google::scp::core::test {
namespace {
class InMemoryMetricExporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    exporter_ = std::make_unique<InMemoryMetricExporter>();
    SetUpResourceMetrics();
  }

  void SetUpResourceMetrics() {
    const opentelemetry::sdk::resource::ResourceAttributes& attributes = {
        {"service.name", "dummy_service"},
        {"service.version", "1.0.0"},
        {"environment", "development"}};

    opentelemetry::sdk::resource::Resource resource =
        opentelemetry::sdk::resource::Resource::Create(
            attributes, "https://example.com/schema");

    resource_ =
        std::make_shared<opentelemetry::sdk::resource::Resource>(resource);

    scope_ =
        opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
            "dummy_scope", "1.0", "https://example.com/schema");

    opentelemetry::sdk::metrics::MetricData metric_data;
    metric_data.instrument_descriptor.name_ = "dummy_metric";
    metric_data.instrument_descriptor.description_ =
        "A dummy metric for testing";
    metric_data.instrument_descriptor.unit_ = "1";
    metric_data.instrument_descriptor.type_ =
        opentelemetry::sdk::metrics::InstrumentType::kCounter;
    metric_data.aggregation_temporality =
        opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
    metric_data.start_ts = opentelemetry::common::SystemTimestamp(
        std::chrono::system_clock::now());
    metric_data.end_ts = opentelemetry::common::SystemTimestamp(
        std::chrono::system_clock::now());

    opentelemetry::sdk::metrics::PointDataAttributes point_data_attr;
    point_data_attr.attributes = {{"fake_key", "fake_value"}};
    point_data_attr.point_data = opentelemetry::sdk::metrics::SumPointData{10};

    metric_data.point_data_attr_.push_back(point_data_attr);

    opentelemetry::sdk::metrics::ScopeMetrics scope_metrics(
        scope_.get(),
        std::vector<opentelemetry::sdk::metrics::MetricData>{metric_data});

    resource_metrics_ =
        std::make_unique<opentelemetry::sdk::metrics::ResourceMetrics>(
            opentelemetry::sdk::metrics::ResourceMetrics(
                resource_.get(),
                std::vector<opentelemetry::sdk::metrics::ScopeMetrics>{
                    scope_metrics}));
  }

  static bool AreResourceMetricsEqual(
      const opentelemetry::sdk::metrics::ResourceMetrics& a,
      const opentelemetry::sdk::metrics::ResourceMetrics& b) {
    if (!AreResourcesEqual(*a.resource_, *b.resource_)) {
      return false;
    }

    const auto& a_scope_metrics = a.scope_metric_data_;
    const auto& b_scope_metrics = b.scope_metric_data_;
    if (a_scope_metrics.size() != b_scope_metrics.size()) {
      return false;
    }

    for (size_t i = 0; i < a_scope_metrics.size(); ++i) {
      if (!AreScopeMetricsEqual(a_scope_metrics[i], b_scope_metrics[i])) {
        return false;
      }
    }
    return true;
  }

  static bool AreResourcesEqual(
      const opentelemetry::sdk::resource::Resource& a,
      const opentelemetry::sdk::resource::Resource& b) {
    if (a.GetSchemaURL() != b.GetSchemaURL()) {
      return false;
    }

    const auto& a_attributes = a.GetAttributes();
    const auto& b_attributes = b.GetAttributes();

    if (a_attributes.size() != b_attributes.size()) {
      return false;
    }

    for (const auto& a_attr : a_attributes) {
      auto b_attr = b_attributes.find(a_attr.first);
      if (b_attr == b_attributes.end()) {
        return false;
      }
      if (a_attr.second != b_attr->second) {
        return false;
      }
    }

    return true;
  }

  static bool AreScopeMetricsEqual(
      const opentelemetry::sdk::metrics::ScopeMetrics& a,
      const opentelemetry::sdk::metrics::ScopeMetrics& b) {
    if (a.scope_ != b.scope_) {
      return false;
    }

    const auto& a_metric_data = a.metric_data_;
    const auto& b_metric_data = b.metric_data_;

    if (a_metric_data.size() != b_metric_data.size()) {
      return false;
    }

    for (size_t i = 0; i < a_metric_data.size(); ++i) {
      if (!AreMetricDataEqual(a_metric_data[i], b_metric_data[i])) {
        return false;
      }
    }
    return true;
  }

  static bool ArePointDataAttributesEqual(
      const opentelemetry::sdk::metrics::PointDataAttributes& a,
      const opentelemetry::sdk::metrics::PointDataAttributes& b) {
    if (a.attributes != b.attributes) {
      return false;
    }
    return true;
  }

  static bool AreMetricDataEqual(
      const opentelemetry::sdk::metrics::MetricData& a,
      const opentelemetry::sdk::metrics::MetricData& b) {
    if (a.aggregation_temporality != b.aggregation_temporality) {
      return false;
    }

    if (a.start_ts != b.start_ts || a.end_ts != b.end_ts) {
      return false;
    }

    const auto& a_point_data_attr = a.point_data_attr_;
    const auto& b_point_data_attr = b.point_data_attr_;

    if (a_point_data_attr.size() != b_point_data_attr.size()) {
      return false;
    }

    for (size_t i = 0; i < a_point_data_attr.size(); ++i) {
      if (!ArePointDataAttributesEqual(a_point_data_attr[i],
                                       b_point_data_attr[i])) {
        return false;
      }
    }
    return true;
  }

  std::unique_ptr<InMemoryMetricExporter> exporter_;
  std::unique_ptr<
      opentelemetry::sdk::instrumentationscope::InstrumentationScope>
      scope_;
  std::unique_ptr<opentelemetry::sdk::metrics::ResourceMetrics>
      resource_metrics_;
  std::shared_ptr<opentelemetry::sdk::resource::Resource> resource_;
};

TEST_F(InMemoryMetricExporterTest, ValidateExportingDummyData) {
  auto result = exporter_->Export(*resource_metrics_);
  ASSERT_EQ(opentelemetry::sdk::common::ExportResult::kSuccess, result);
  const std::vector<opentelemetry::sdk::metrics::ResourceMetrics>&
      exported_data = exporter_->data();

  ASSERT_EQ(1, exported_data.size());

  const opentelemetry::sdk::metrics::ResourceMetrics& exported_resource_metric =
      exported_data[0];

  ASSERT_TRUE(
      AreResourceMetricsEqual(exported_resource_metric, *resource_metrics_));
}

TEST_F(InMemoryMetricExporterTest, ValidateShutdown) {
  bool shutdown_result = exporter_->Shutdown();

  ASSERT_TRUE(shutdown_result);

  ASSERT_TRUE(exporter_->is_shutdown());
}

TEST_F(InMemoryMetricExporterTest, ValidateForceFlush) {
  bool flush_result = exporter_->ForceFlush();

  ASSERT_TRUE(flush_result);
}

TEST_F(InMemoryMetricExporterTest, ValidateAggregationTemporality) {
  ASSERT_EQ(exporter_->GetAggregationTemporality(
                opentelemetry::sdk::metrics::InstrumentType::kCounter),
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative);
}
}  // namespace
}  // namespace google::scp::core::test
