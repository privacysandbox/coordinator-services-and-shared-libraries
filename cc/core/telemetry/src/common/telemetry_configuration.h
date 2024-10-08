/*
 * Copyright 2024 Google LLC
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

#pragma once

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "cc/core/interface/config_provider_interface.h"

namespace google::scp::core {
// OTel metrics exporter config
//
// Supported values for `OTEL_METRICS_EXPORTER` are:
// - "otlp": OpenTelemetry Protocol to collector
// - "googlecloud": MonitoringExporter to Google Cloud Monitoring
inline constexpr absl::string_view kOtelMetricsExporterKey =
    "OTEL_METRICS_EXPORTER";
inline constexpr absl::string_view kOtelMetricsExporterValue = "otlp";

// Path to otel collector
inline constexpr absl::string_view kOtelExporterOtlpEndpointKey =
    "google_scp_otel_exporter_otlp_endpoint";
inline constexpr absl::string_view kOtelExporterOtlpEndpointValue =
    "127.0.0.1:4317";

// Config to check if we can use otel for metric collection
inline constexpr absl::string_view kUseOtelForMetricCollectionKey =
    "google_scp_use_otel_for_metric_collection";
inline constexpr bool kUseOtelForMetricCollectionValue = false;

// Metric export interval
// Defaults to 60s
inline constexpr absl::string_view kOtelMetricExportIntervalMsecKey =
    "google_scp_otel_metric_export_interval_msec";
inline constexpr int32_t kOtelMetricExportIntervalMsecValue = 60000;

// Metric export timeout
// Defaults to 20s
inline constexpr absl::string_view kOtelMetricExportTimeoutMsecKey =
    "google_scp_otel_metric_export_timeout_msec";
inline constexpr int32_t kOtelMetricExportTimeoutMsecValue = 20000;

// Service Account
inline constexpr absl::string_view kOtelServiceAccountKey =
    "google_scp_otel_service_account";
inline constexpr absl::string_view kOtelServiceAccountValue = "";

// Audience
inline constexpr absl::string_view kOtelAudienceKey =
    "google_scp_otel_audience";
inline constexpr absl::string_view kOtelAudienceValue = "";

// Cred config path
// defaults to empty string for local and gcp cases, non-empty in the case of
// AWS
// https://google.aip.dev/auth/4117
// https://cloud.google.com/iam/docs/workload-identity-federation-with-other-clouds#create-cred-config
// run this command presented here:
// https://cloud.google.com/sdk/gcloud/reference/iam/workload-identity-pools/create-cred-config
// and set the config
inline constexpr absl::string_view kOtelCredConfigKey =
    "google_scp_otel_cred_config";
inline constexpr absl::string_view kOtelCredConfigValue = "";

template <typename T>
T GetConfigValue(const std::string& key, const T& default_value,
                 ConfigProviderInterface& config_provider) {
  T result = default_value;
  auto execution_result = config_provider.Get(key, result);
  if (!execution_result.Successful()) {
    result = default_value;
  }
  return result;
}
}  // namespace google::scp::core
