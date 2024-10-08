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
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_context.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/view/view.h"
#include "opentelemetry/sdk/metrics/view/view_factory.h"
#include "opentelemetry/sdk/metrics/view/view_registry_factory.h"
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

TEST_F(MetricRouterTest, GetOrCreateMeterCreatesNewMeterIfNotExist) {
  absl::string_view service_name = "test_service";
  absl::string_view version = "1.0";
  absl::string_view schema_url = "http://example.com/schema";

  auto meter =
      metric_router_->GetOrCreateMeter(service_name, version, schema_url);

  EXPECT_NE(meter, nullptr);
}

TEST_F(MetricRouterTest, GetOrCreateMeterReturnsExistingMeter) {
  absl::string_view service_name = "test_service";
  absl::string_view version = "1.0";
  absl::string_view schema_url = "http://example.com/schema";

  auto meter1 =
      metric_router_->GetOrCreateMeter(service_name, version, schema_url);

  auto meter2 =
      metric_router_->GetOrCreateMeter(service_name, version, schema_url);

  EXPECT_EQ(meter1, meter2);
}

TEST_F(MetricRouterTest, GetOrCreateMeterIsThreadSafe) {
  absl::string_view service_name1 = "test_service_1";
  absl::string_view service_name2 = "test_service_2";
  absl::string_view version = "1.0";
  absl::string_view schema_url = "http://example.com/schema";

  // Run GetOrCreateMeter in parallel to test thread safety.
  std::thread t1([&]() {
    metric_router_->GetOrCreateMeter(service_name1, version, schema_url);
  });
  std::thread t2([&]() {
    metric_router_->GetOrCreateMeter(service_name2, version, schema_url);
  });

  t1.join();
  t2.join();

  // Check that meters were created without any race conditions.
  EXPECT_NE(
      metric_router_->GetOrCreateMeter(service_name1, version, schema_url),
      nullptr);
  EXPECT_NE(
      metric_router_->GetOrCreateMeter(service_name2, version, schema_url),
      nullptr);
}

TEST_F(MetricRouterTest,
       GetOrCreateSyncInstrumentCreatesNewInstrumentWhenNotInCache) {
  auto meter =
      metric_router_->GetOrCreateMeter("test_service", "1.0", "schema_url");

  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()>
      factory = [&]()
      -> std::shared_ptr<opentelemetry::metrics::SynchronousInstrument> {
    return meter->CreateUInt64Counter("test_counter", "Test description",
                                      "unit");
  };
  auto instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_counter", std::move(factory));

  // Ensure the instrument is created and returned.
  EXPECT_NE(instrument, nullptr);

  // Check if the instrument is stored in the cache.
  auto cached_instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_counter", std::move(factory));
  EXPECT_EQ(instrument, cached_instrument);
}

TEST_F(MetricRouterTest,
       GetOrCreateSyncInstrumentCreatesNewHistogramWhenNotInCache) {
  auto meter =
      metric_router_->GetOrCreateMeter("test_service", "1.0", "schema_url");

  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()>
      factory = [&]()
      -> std::unique_ptr<opentelemetry::metrics::Histogram<uint64_t>> {
    return meter->CreateUInt64Histogram("test_histogram", "Test description",
                                        "unit");
  };

  auto instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_histogram", std::move(factory));

  // Ensure the instrument is created and returned.
  EXPECT_NE(instrument, nullptr);

  // Check if the instrument is stored in the cache.
  auto cached_instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_histogram", std::move(factory));
  EXPECT_EQ(instrument, cached_instrument);
}

TEST_F(MetricRouterTest,
       GetOrCreateSyncInstrumentCreatesNewDoubleCounterWhenNotInCache) {
  auto meter =
      metric_router_->GetOrCreateMeter("test_service", "1.0", "schema_url");

  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()>
      factory =
          [&]() -> std::unique_ptr<opentelemetry::metrics::Counter<double>> {
    return meter->CreateDoubleCounter("test_double_counter", "Test description",
                                      "unit");
  };

  auto instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_double_counter", std::move(factory));

  // Ensure the instrument is created and returned.
  EXPECT_NE(instrument, nullptr);

  // Check if the instrument is stored in the cache.
  auto cached_instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_double_counter", std::move(factory));
  EXPECT_EQ(instrument, cached_instrument);
}

TEST_F(MetricRouterTest,
       GetOrCreateSyncInstrumentCreatesNewDoubleHistogramWhenNotInCache) {
  auto meter =
      metric_router_->GetOrCreateMeter("test_service", "1.0", "schema_url");

  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()>
      factory =
          [&]() -> std::unique_ptr<opentelemetry::metrics::Histogram<double>> {
    return meter->CreateDoubleHistogram("test_double_histogram",
                                        "Test description", "unit");
  };

  auto instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_double_histogram", std::move(factory));

  // Ensure the instrument is created and returned.
  EXPECT_NE(instrument, nullptr);

  // Check if the instrument is stored in the cache.
  auto cached_instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_double_histogram", std::move(factory));
  EXPECT_EQ(instrument, cached_instrument);
}

TEST_F(MetricRouterTest,
       GetOrCreateSyncInstrumentReturnsCachedInstrumentWhenAlreadyInCache) {
  auto meter =
      metric_router_->GetOrCreateMeter("test_service", "1.0", "schema_url");

  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()>
      factory =
          [&]() -> std::unique_ptr<opentelemetry::metrics::Counter<uint64_t>> {
    return meter->CreateUInt64Counter("cached_counter", "Test description",
                                      "unit");
  };

  auto instrument = metric_router_->GetOrCreateSyncInstrument(
      "cached_counter", std::move(factory));
  EXPECT_NE(instrument, nullptr);

  // Try fetching the cached instrument again.
  auto cached_instrument = metric_router_->GetOrCreateSyncInstrument(
      "cached_counter", std::move(factory));

  // Ensure the same instance is returned.
  EXPECT_EQ(instrument, cached_instrument);
}

TEST_F(MetricRouterTest, GetOrCreateSyncInstrumentIsThreadSafe) {
  absl::string_view service_name = "test_service";
  absl::string_view version = "1.0";
  absl::string_view schema_url = "http://example.com/schema";

  auto meter =
      metric_router_->GetOrCreateMeter(service_name, version, schema_url);

  // Define the factory to create a new instrument.
  auto create_factory = [&meter]()
      -> absl::AnyInvocable<
          std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()> {
    return
        [meter]()
            -> std::shared_ptr<opentelemetry::metrics::SynchronousInstrument> {
          return meter->CreateUInt64Counter("test_counter", "Test description",
                                            "unit");
        };
  };

  // Run GetOrCreateSyncInstrument in parallel to test thread safety.
  std::thread t1([&]() {
    auto factory = create_factory();
    metric_router_->GetOrCreateSyncInstrument("test_counter",
                                              std::move(factory));
  });
  std::thread t2([&]() {
    auto factory = create_factory();
    metric_router_->GetOrCreateSyncInstrument("test_counter",
                                              std::move(factory));
  });

  t1.join();
  t2.join();

  // Create a new factory for the final call.
  auto factory = create_factory();
  // Check that the instrument was created without any race conditions.
  auto instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_counter", std::move(factory));
  EXPECT_NE(instrument, nullptr);

  // Create another factory for this call. We cannot use
  auto cached_factory = create_factory();
  // Ensure that the same instrument is returned from the cache.
  auto cached_instrument = metric_router_->GetOrCreateSyncInstrument(
      "test_counter", std::move(cached_factory));
  EXPECT_EQ(instrument, cached_instrument);
}

TEST_F(MetricRouterTest, CreatesNewObservableGaugeWhenNotInCache) {
  absl::string_view service_name = "test_service";
  absl::string_view version = "1.0";
  absl::string_view schema_url = "http://example.com/schema";
  absl::string_view metric_name = "new_gauge";
  absl::string_view description = "A new test gauge";
  absl::string_view unit = "percent";

  auto meter =
      metric_router_->GetOrCreateMeter(service_name, version, schema_url);

  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()>
      factory = [&]()
      -> std::shared_ptr<opentelemetry::metrics::ObservableInstrument> {
    return meter->CreateDoubleObservableGauge(metric_name, description, unit);
  };

  auto result = metric_router_->GetOrCreateObservableInstrument(
      metric_name, std::move(factory));

  // Check that the result is not null.
  EXPECT_NE(result, nullptr);
}

TEST_F(MetricRouterTest, ReturnsExistingObservableGauge) {
  absl::string_view service_name = "test_service";
  absl::string_view version = "1.0";
  absl::string_view schema_url = "http://example.com/schema";
  absl::string_view metric_name = "existing_gauge";
  absl::string_view description = "An existing test gauge";
  absl::string_view unit = "unit";

  auto meter =
      metric_router_->GetOrCreateMeter(service_name, version, schema_url);

  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()>
      factory = [&]()
      -> std::shared_ptr<opentelemetry::metrics::ObservableInstrument> {
    return meter->CreateDoubleObservableGauge(metric_name, description, unit);
  };

  // First call to create the gauge.
  auto result1 = metric_router_->GetOrCreateObservableInstrument(
      metric_name, std::move(factory));

  // Second call should return the same gauge.
  auto result2 = metric_router_->GetOrCreateObservableInstrument(
      metric_name, std::move(factory));

  EXPECT_EQ(result1, result2);
}

TEST_F(MetricRouterTest, CreatesDifferentTypesOfInstruments) {
  absl::string_view service_name = "test_service";
  absl::string_view version = "1.0";
  absl::string_view schema_url = "http://example.com/schema";

  auto meter =
      metric_router_->GetOrCreateMeter(service_name, version, schema_url);

  // Define factories for different types of observable instruments.
  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()>
      double_gauge_factory = [&]()
      -> std::shared_ptr<opentelemetry::metrics::ObservableInstrument> {
    return meter->CreateDoubleObservableGauge(
        "double_gauge", "Double gauge description", "unit");
  };

  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()>
      int_gauge_factory = [&]()
      -> std::shared_ptr<opentelemetry::metrics::ObservableInstrument> {
    return meter->CreateInt64ObservableGauge("int_gauge",
                                             "Int gauge description", "unit");
  };

  // Create and check DoubleObservableGauge.
  auto double_gauge = metric_router_->GetOrCreateObservableInstrument(
      "double_gauge", std::move(double_gauge_factory));
  EXPECT_NE(double_gauge, nullptr);

  // Create and check Int64ObservableGauge.
  auto int_gauge = metric_router_->GetOrCreateObservableInstrument(
      "int_gauge", std::move(int_gauge_factory));
  EXPECT_NE(int_gauge, nullptr);
}

TEST_F(MetricRouterTest, HandlesInvalidObservableInstrumentType) {
  absl::string_view invalid_metric_name = "invalid_metric";

  // Define a factory for an unsupported type.
  absl::AnyInvocable<
      std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()>
      invalid_factory = [&]()
      -> std::shared_ptr<opentelemetry::metrics::ObservableInstrument> {
    // This should simulate the creation of an invalid type, e.g., float.
    return nullptr;
  };

  auto result = metric_router_->GetOrCreateObservableInstrument(
      invalid_metric_name, std::move(invalid_factory));

  // Expecting a null pointer because the type should be unsupported.
  EXPECT_EQ(result, nullptr);
}

TEST_F(MetricRouterTest, CreateHistogramViewForInstrumentReturnsSuccess) {
  std::string metric_name = "test_metric";
  std::string view_name = "test_view";
  std::string description = "Test histogram view";
  std::string unit = "ms";
  MetricRouter::InstrumentType instrument_type =
      MetricRouter::InstrumentType::kHistogram;
  std::vector<double> boundaries = {0.0, 10.0, 20.0};

  auto execution_result = metric_router_->CreateHistogramViewForInstrument(
      metric_name, view_name, instrument_type, boundaries, "1.0",
      "http://example.com/schema", description, unit);

  EXPECT_EQ(execution_result, SuccessExecutionResult());
}

TEST_F(MetricRouterTest,
       CreateHistogramViewForInstrumentWithNoopMeterProvider) {
  std::string metric_name = "test_metric";
  std::string view_name = "test_view";
  std::string description = "Test histogram view";
  std::string unit = "ms";
  MetricRouter::InstrumentType instrument_type =
      MetricRouter::InstrumentType::kHistogram;
  std::vector<double> boundaries = {0.0, 10.0, 20.0};

  // Ensure that a NoopMeterProvider is being used.
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
      meter_provider =
          std::make_shared<opentelemetry::metrics::NoopMeterProvider>();
  opentelemetry::metrics::Provider::SetMeterProvider(meter_provider);

  auto execution_result = metric_router_->CreateHistogramViewForInstrument(
      metric_name, view_name, instrument_type, boundaries, "1.0",
      "http://example.com/schema", description, unit);

  EXPECT_EQ(execution_result, FailureExecutionResult(
                                  SC_TELEMETRY_METER_PROVIDER_NOT_INITIALIZED));
}

}  // namespace
}  // namespace google::scp::core::test
