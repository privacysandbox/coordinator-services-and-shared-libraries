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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/telemetry/src/metric/error_codes.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/resource/resource.h"

/**
 * @brief MetricRouter class for handling OpenTelemetry metric.
 * The MetricRouter class is responsible for managing an OpenTelemetry
 * MeterProvider and provides access to it.
 */
namespace google::scp::core {

class MetricRouter {
 public:
  /**
   * Create a MetricRouter with a Periodic Reader, given a Resource,
   * an Exporter, and a ConfigProvider.
   *
   * @param config_provider Shared pointer to the configuration provider
   * @param resource The OpenTelemetry resource
   * @param exporter Unique pointer to the push metric exporter
   */
  explicit MetricRouter(
      std::shared_ptr<ConfigProviderInterface> config_provider,
      opentelemetry::sdk::resource::Resource resource,
      std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
          exporter);

  /**
   * Gets an existing Meter or creates a new one if it doesn't exist.
   *
   * This function is thread-safe.
   *
   * @param service_name The name of the service(meter name) for which to get or
   * create a Meter
   * @param version The version of the service (default is "1.0")
   * @param schema_url The schema URL (default is empty string)
   * @return A shared pointer to the Meter for the specified service
   */
  std::shared_ptr<opentelemetry::metrics::Meter> GetOrCreateMeter(
      absl::string_view service_name, absl::string_view version = "1.0",
      absl::string_view schema_url = "");

  /**
   * Gets an existing SynchronousInstrument or creates a new one if it doesn't
   * exist.
   *
   * This function is thread-safe.
   *
   * @param metric_name The name of the metric for which to get or create a
   * SynchronousInstrument
   * @param instrument_factory A callable that creates a new
   * SynchronousInstrument if needed
   * @return A shared pointer to the SynchronousInstrument for the specified
   * metric
   */
  std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>
  GetOrCreateSyncInstrument(
      absl::string_view metric_name,
      absl::AnyInvocable<
          std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>()>
          instrument_factory);

  /**
   * Gets an existing ObservableInstrument or creates a new one if it doesn't
   * exist.
   *
   * This function is thread-safe.
   *
   * @warning When adding a callback to the Instrument using
   * ObservableInstrument::AddCallback, it is important to remove the callback
   * before the observed object gets destroyed using
   * ObservableInstrument::RemoveCallback to avoid a use-after-free error.
   *
   * @param metric_name The name of the metric for which to get or create an
   * ObservableInstrument
   * @param instrument_factory A callable that creates a new
   * ObservableInstrument if needed
   * @return A shared pointer to the ObservableInstrument for the specified
   * metric
   */
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
  GetOrCreateObservableInstrument(
      absl::string_view metric_name,
      absl::AnyInvocable<
          std::shared_ptr<opentelemetry::metrics::ObservableInstrument>()>
          instrument_factory);

  /**
   * @brief Creates a view for an instrument, defining how data from the
   * instrument will be aggregated and viewed. View for instrument should be
   * setup before the instrument is created.
   *
   * This function configures a new view for a specified instrument within a
   * given meter. To correctly associate the view with the intended instrument
   * and meter, ensure that the exact name, version, and schema of the meter,
   * along with the exact instrument name, unit, and type are provided. Without
   * these exact matches, the view will not be correctly assigned to the
   * instrument (of the meter), resulting in the instrument using the default
   * view configuration with boundaries: [0, 5, 10, 25, 50, 75, 100, 250, 500,
   * 750, 1000, 2500, 5000, 7500, 10000].
   *
   * @param meter_name         The exact name of the meter to which the
   * instrument belongs.
   * @param instrument_name    The exact name of the instrument for which the
   * view is being created, such as a counter or histogram. This allows specific
   * configuration of aggregation for individual instruments.
   * @param instrument_type    The type of the instrument, such as Counter,
   * Histogram, UpDownCounter, ObservableCounter, ObservableGauge,
   * ObservableUpDownCounter, defining the kind of data this instrument will
   * handle.
   * @param aggregation_type    The type of the aggregation, such as Drop,
   * Histogram, LastValue, Sum, Default, defining the kind of data
   * aggregation for this this instrument's data.
   * @param boundaries         A vector of boundary values for aggregation. Used
   * primarily with histogram-type instruments to define bucket boundaries for
   * data distribution.
   * @param version            (Optional) The exact version of the meter to
   * match, defaulting to "1.0" if unspecified. Used to ensure compatibility of
   * the view with the meter’s version.
   * @param schema_url             (Optional) The exact schema of the meter,
   * helping standardize the structure and meaning of metrics collected. It is
   * part of creating a Meter, defaults to an empty string, and is mostly empty.
   * @param view_description   (Optional) A description of the view, explaining
   * the purpose or scope of this view configuration.
   * @param unit               (Optional) The exact unit of measurement for the
   * instrument (e.g., "seconds", "bytes"). Providing the correct unit helps
   * associate the view with the instrument's data.
   *
   * @return ExecutionResult   Returns an `ExecutionResult` indicating success
   * or failure in creating the view. This helps manage error handling and
   * reporting in the view creation process.
   */
  ExecutionResult CreateViewForInstrument(
      absl::string_view meter_name, absl::string_view instrument_name,
      opentelemetry::sdk::metrics::InstrumentType instrument_type,
      opentelemetry::sdk::metrics::AggregationType aggregation_type,
      const std::vector<double>& boundaries, absl::string_view version = "1.0",
      absl::string_view schema_url = "",
      absl::string_view view_description = "", absl::string_view unit = "");

 protected:
  MetricRouter() = default;

  /**
   * Sets up the MetricRouter with custom Resource and MetricReader.
   *
   * This function is intended for use in subclasses during testing to allow
   * customization of the Resource and MetricReader.
   *
   * @param resource The custom Resource to be used in the MeterContext
   * @param metric_reader The custom MetricReader to be added to the
   * MeterContext
   */
  void SetupMetricRouter(
      opentelemetry::sdk::resource::Resource resource,
      std::shared_ptr<opentelemetry::sdk::metrics::MetricReader> metric_reader);

 private:
  /**
   * Creates a PeriodicExportingMetricReader with configured export interval and
   * timeout.
   *
   * @param config_provider A shared pointer to the configuration provider used
   * to retrieve export settings
   * @param exporter A unique pointer to the PushMetricExporter to be used by
   * the reader
   * @return A shared pointer to the created PeriodicExportingMetricReader
   */
  static std::shared_ptr<opentelemetry::sdk::metrics::MetricReader>
  CreatePeriodicReader(
      std::shared_ptr<ConfigProviderInterface> config_provider,
      std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
          exporter);

  absl::flat_hash_map<std::string,
                      std::shared_ptr<opentelemetry::metrics::Meter>>
      meters_;
  absl::flat_hash_map<
      std::string,
      std::shared_ptr<opentelemetry::metrics::SynchronousInstrument>>
      synchronous_instruments_;
  absl::flat_hash_map<
      std::string,
      std::shared_ptr<opentelemetry::metrics::ObservableInstrument>>
      asynchronous_instruments_;

  mutable absl::Mutex metric_mutex_;
};

}  // namespace google::scp::core
