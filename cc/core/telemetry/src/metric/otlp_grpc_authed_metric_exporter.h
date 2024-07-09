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

#include <chrono>
#include <memory>

#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/telemetry/src/authentication/grpc_auth_config.h"
#include "cc/core/telemetry/src/authentication/grpc_id_token_authenticator.h"
#include "cc/core/telemetry/src/authentication/token_fetcher.h"
#include "opentelemetry/common/spin_lock_mutex.h"
#include "opentelemetry/exporters/otlp/otlp_environment.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h"
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"

namespace google::scp::core {

/**
 * The OtlpGrpcAuthedMetricExporter is largely copied from
 OtlpGrpcMetricExporter,
 * as we need an OtlpGrpcMetricExporter with custom authentication logic.

 * Like OtlpGrpcMetricExporter, the OtlpGrpcAuthedMetricExporter exports metric
 * data in OpenTelemetry Protocol (OTLP) format in gRPC; in addition, it fetches
 * GCP ID tokens needed for authentication, and manages their expiry.
 */
class OtlpGrpcAuthedMetricExporter
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  explicit OtlpGrpcAuthedMetricExporter(
      const opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions&
          options,
      std::unique_ptr<GrpcIdTokenAuthenticator> grpc_id_token_authenticator);

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType instrument_type)
      const noexcept override;

  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::sdk::metrics::ResourceMetrics& data) noexcept
      override;

  bool ForceFlush(std::chrono::microseconds timeout =
                      (std::chrono::microseconds::max)()) noexcept override;

  bool Shutdown(std::chrono::microseconds timeout =
                    (std::chrono::microseconds::max)()) noexcept override;

  const opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions&
  GetOptions() const;

 private:
  bool IsShutdown() const noexcept;

  ExecutionResultOr<std::shared_ptr<grpc::Channel>> MakeChannel(
      const opentelemetry::exporter::otlp::OtlpGrpcClientOptions& options,
      std::unique_ptr<GrpcIdTokenAuthenticator> grpc_id_token_authenticator);

  ExecutionResultOr<std::unique_ptr<opentelemetry::proto::collector::metrics::
                                        v1::MetricsService::StubInterface>>
  MakeMetricsServiceStub(
      const opentelemetry::exporter::otlp::OtlpGrpcClientOptions& options,
      std::unique_ptr<GrpcIdTokenAuthenticator> grpc_id_token_authenticator);

  const opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions options_;
  const opentelemetry::sdk::metrics::AggregationTemporalitySelector
      aggregation_temporality_selector_;
  std::unique_ptr<opentelemetry::proto::collector::metrics::v1::MetricsService::
                      StubInterface>
      metrics_service_stub_;
  bool is_shutdown_;
  mutable std::mutex mutex_;
};
}  // namespace google::scp::core
