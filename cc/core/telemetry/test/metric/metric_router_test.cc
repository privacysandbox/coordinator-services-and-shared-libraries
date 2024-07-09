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

#include "core/telemetry/src/metric/metric_router.h"

#include <memory>

#include "core/config_provider/mock/mock_config_provider.h"
#include "core/telemetry/mock/in_memory_metric_exporter.h"
#include "core/telemetry/mock/in_memory_metric_reader.h"
#include "core/telemetry/src/common/telemetry_configuration.h"
#include "include/gtest/gtest.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace google::scp::core::test {
namespace {
class MetricRouterTest : public testing::Test {
 protected:
  void SetUp() override {
    exporter_ = std::make_unique<InMemoryMetricExporter>();
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
        mock_config_provider_,
        opentelemetry::sdk::resource::Resource::GetDefault(),
        std::move(exporter_));
  }

  std::shared_ptr<config_provider::mock::MockConfigProvider>
      mock_config_provider_;
  std::unique_ptr<google::scp::core::MetricRouter> metric_router_;
  std::unique_ptr<InMemoryMetricExporter> exporter_;
  std::shared_ptr<InMemoryMetricReader> reader_;
};

TEST_F(MetricRouterTest, ConstructorAndGetMeterProvider) {
  std::shared_ptr<opentelemetry::metrics::MeterProvider> meter_provider =
      opentelemetry::metrics::Provider::GetMeterProvider();

  ASSERT_NE(nullptr, meter_provider);
  EXPECT_NE(meter_provider->GetMeter("test"), nullptr);
}
}  // namespace
}  // namespace google::scp::core::test
