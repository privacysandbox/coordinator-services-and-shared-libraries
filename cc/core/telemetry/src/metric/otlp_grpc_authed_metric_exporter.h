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
#include "external/com_github_opentelemetry_proto/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "external/io_opentelemetry_cpp/api/_virtual_includes/api/opentelemetry/common/spin_lock_mutex.h"
#include "external/io_opentelemetry_cpp/exporters/otlp/_virtual_includes/otlp_grpc_exporter/opentelemetry/exporters/otlp/otlp_environment.h"
#include "external/io_opentelemetry_cpp/exporters/otlp/_virtual_includes/otlp_grpc_exporter/opentelemetry/exporters/otlp/protobuf_include_prefix.h"
#include "external/io_opentelemetry_cpp/exporters/otlp/_virtual_includes/otlp_grpc_exporter/opentelemetry/exporters/otlp/protobuf_include_suffix.h"
#include "external/io_opentelemetry_cpp/exporters/otlp/_virtual_includes/otlp_grpc_metric_exporter/opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h"
#include "external/io_opentelemetry_cpp/sdk/_virtual_includes/headers/opentelemetry/sdk/metrics/push_metric_exporter.h"

namespace google::scp::core {

/**
 * The OTLP exporter exports metric data in OpenTelemetry Protocol (OTLP)
 * format in gRPC.
 *
 * It also fetches the Id token which is needed for the authentication. It
 * manages the expiry of the tokens as well
 */
class OtlpGrpcAuthedMetricExporter
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  explicit OtlpGrpcAuthedMetricExporter(
      const opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions&
          options,
      std::unique_ptr<GrpcIdTokenAuthenticator>
          grpc_id_token_authenticator);

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
      std::unique_ptr<GrpcIdTokenAuthenticator>
          grpc_id_token_authenticator);

  ExecutionResultOr<std::unique_ptr<opentelemetry::proto::collector::metrics::
                                        v1::MetricsService::StubInterface>>
  MakeMetricsServiceStub(
      const opentelemetry::exporter::otlp::OtlpGrpcClientOptions& options,
      std::unique_ptr<GrpcIdTokenAuthenticator>
          grpc_id_token_authenticator);

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
