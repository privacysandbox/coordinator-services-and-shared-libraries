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

#include "core/telemetry/src/metric/otlp_grpc_authed_metric_exporter.h"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/telemetry/src/authentication/gcp_token_fetcher.h"
#include "core/telemetry/src/metric/error_codes.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_client.h"
#include "opentelemetry/exporters/otlp/otlp_metric_utils.h"
#include "opentelemetry/ext/http/common/url_parser.h"

namespace google::scp::core {
inline constexpr absl::string_view kOtlpGrpcAuthedExporter =
    "OtlpGrpcAuthedExporter";

/**
 * Creates a gRPC channel using the provided OTLP gRPC client options and a
 * configuration provider.
 *
 * Strips the scheme from the endpoint and sets up channel credentials based on
 * the provided configuration. Returns nullptr if the endpoint is invalid or if
 * there are issues creating the channel.
 */
ExecutionResultOr<std::shared_ptr<grpc::Channel>>
OtlpGrpcAuthedMetricExporter::MakeChannel(
    const opentelemetry::exporter::otlp::OtlpGrpcClientOptions& options,
    std::unique_ptr<GrpcIdTokenAuthenticator> grpc_id_token_authenticator) {
  // Scheme is allowed in OTLP endpoint definition, but is not allowed for
  // creating gRPC channel. Passing URI with scheme to grpc::CreateChannel could
  // resolve the endpoint to some unexpected address.
  opentelemetry::ext::http::common::UrlParser url(options.endpoint);
  if (!url.success_) {
    auto execution_result =
        FailureExecutionResult(SC_TELEMETRY_COULD_NOT_PARSE_URL);
    SCP_ERROR(kOtlpGrpcAuthedExporter, google::scp::core::common::kZeroUuid,
              execution_result, "[OTLP GRPC Client] invalid endpoint: %s",
              options.endpoint.c_str());

    return FailureExecutionResult(SC_TELEMETRY_COULD_NOT_PARSE_URL);
  }

  std::string grpc_target =
      absl::StrCat(url.host_, ":", static_cast<int>(url.port_));
  grpc::ChannelArguments grpc_arguments;
  grpc_arguments.SetUserAgentPrefix(options.user_agent);

  std::shared_ptr<grpc::ChannelCredentials> channel_credentials =
      grpc::InsecureChannelCredentials();

  if (grpc_id_token_authenticator &&
      grpc_id_token_authenticator->auth_config() &&
      grpc_id_token_authenticator->auth_config()->IsValid()) {
    auto call_creds = grpc::MetadataCredentialsFromPlugin(
        std::unique_ptr<grpc::MetadataCredentialsPlugin>(
            std::move(grpc_id_token_authenticator)));

    auto ssl_channel_creds =
        grpc::SslCredentials(grpc::SslCredentialsOptions());

    channel_credentials =
        grpc::CompositeChannelCredentials(ssl_channel_creds, call_creds);
  }
  return grpc::CreateCustomChannel(grpc_target, channel_credentials,
                                   grpc_arguments);
}

/**
 * Creates a stub interface for the OTLP MetricsService.
 *
 * This function uses the provided OTLP gRPC client options and configuration
 * provider to create a gRPC channel, and then creates a stub for the
 * MetricsService using that channel. If the channel creation fails, this
 * function returns nullptr.
 */
ExecutionResultOr<std::unique_ptr<opentelemetry::proto::collector::metrics::v1::
                                      MetricsService::StubInterface>>
OtlpGrpcAuthedMetricExporter::MakeMetricsServiceStub(
    const opentelemetry::exporter::otlp::OtlpGrpcClientOptions& options,
    std::unique_ptr<GrpcIdTokenAuthenticator> grpc_id_token_authenticator) {
  ExecutionResultOr<std::shared_ptr<grpc::Channel>> channel =
      MakeChannel(options, std::move(grpc_id_token_authenticator));
  if (!channel.Successful()) {
    auto execution_result =
        FailureExecutionResult(SC_TELEMETRY_GRPC_CHANNEL_CREATION_FAILED);
    SCP_ERROR(kOtlpGrpcAuthedExporter, google::scp::core::common::kZeroUuid,
              execution_result,
              "[OTLP METRIC GRPC Exporter] Grpc channel creation failed! Could "
              "not create a metric service stub!");
    return FailureExecutionResult(SC_TELEMETRY_GRPC_CHANNEL_CREATION_FAILED);
  }
  return opentelemetry::proto::collector::metrics::v1::MetricsService::NewStub(
      *channel);
}

OtlpGrpcAuthedMetricExporter::OtlpGrpcAuthedMetricExporter(
    const opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions& options,
    std::unique_ptr<GrpcIdTokenAuthenticator> grpc_id_token_authenticator)
    : options_(options),
      aggregation_temporality_selector_{
          opentelemetry::exporter::otlp::OtlpMetricUtils::
              ChooseTemporalitySelector(options_.aggregation_temporality)},
      is_shutdown_(false) {
  auto metrics_service_stub =
      MakeMetricsServiceStub(options_, std::move(grpc_id_token_authenticator));
  if (metrics_service_stub.Successful()) {
    metrics_service_stub_ = metrics_service_stub.release();
  } else {
    metrics_service_stub_ = nullptr;
  }
}

opentelemetry::sdk::metrics::AggregationTemporality
OtlpGrpcAuthedMetricExporter::GetAggregationTemporality(
    opentelemetry::sdk::metrics::InstrumentType instrument_type)
    const noexcept {
  return aggregation_temporality_selector_(instrument_type);
}

opentelemetry::sdk::common::ExportResult OtlpGrpcAuthedMetricExporter::Export(
    const opentelemetry::sdk::metrics::ResourceMetrics& data) noexcept {
  if (IsShutdown()) {
    auto execution_result =
        FailureExecutionResult(SC_TELEMETRY_EXPORTER_SHUTDOWN);
    SCP_ERROR(kOtlpGrpcAuthedExporter, google::scp::core::common::kZeroUuid,
              execution_result,
              "[OTLP METRIC GRPC Exporter] Exporting %s metric(s) failed, "
              "exporter is shutdown",
              data.scope_metric_data_.size())
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  // no data to export
  if (data.scope_metric_data_.empty()) {
    return opentelemetry::sdk::common::ExportResult::kSuccess;
  }

  opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest
      request;
  opentelemetry::exporter::otlp::OtlpMetricUtils::PopulateRequest(data,
                                                                  &request);

  auto context =
      opentelemetry::exporter::otlp::OtlpGrpcClient::MakeClientContext(
          options_);
  opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse
      response;

  if (metrics_service_stub_ == nullptr) {
    auto execution_result = FailureExecutionResult(SC_TELEMETRY_EXPORT_FAILED);
    SCP_ERROR(kOtlpGrpcAuthedExporter, google::scp::core::common::kZeroUuid,
              execution_result,
              "[OTLP METRIC GRPC Exporter] Export() failed because metric "
              "service stub is not properly set");

    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  google::protobuf::ArenaOptions arena_options;
  // It's easy to allocate data blocks larger than 1024 when we populate them
  // with basic kresource and attributes.
  arena_options.initial_block_size = 1024;  // 1KB
  // When in batch mode, it's easy to export a large number of spans at once; we
  // can alloc a larger block to reduce memory fragments.
  arena_options.max_block_size = 65536;  // 64KB
  std::unique_ptr<google::protobuf::Arena> arena =
      std::make_unique<google::protobuf::Arena>(arena_options);

  grpc::Status status =
      opentelemetry::exporter::otlp::OtlpGrpcClient::DelegateExport(
          metrics_service_stub_.get(), std::move(context), std::move(arena),
          std::move(request), &response);

  if (!status.ok()) {
    auto execution_result = FailureExecutionResult(SC_TELEMETRY_EXPORT_FAILED);
    SCP_ERROR(kOtlpGrpcAuthedExporter, google::scp::core::common::kZeroUuid,
              execution_result,
              "[OTLP METRIC GRPC Exporter] Export() failed: %s",
              status.error_message().c_str());

    return opentelemetry::sdk::common::ExportResult::kFailure;
  }
  return opentelemetry::sdk::common::ExportResult::kSuccess;
}

// Unimplemented, following the reference from the official docs,
// https://github.com/open-telemetry/opentelemetry-cpp/tree/c7a88c479fba3c7ee039e426ba6085b344a8759a/exporters
bool OtlpGrpcAuthedMetricExporter::ForceFlush(
    std::chrono::microseconds) noexcept {
  return true;
}

bool OtlpGrpcAuthedMetricExporter::Shutdown(
    std::chrono::microseconds) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  is_shutdown_ = true;
  return true;
}

bool OtlpGrpcAuthedMetricExporter::IsShutdown() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return is_shutdown_;
}

const opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions&
OtlpGrpcAuthedMetricExporter::GetOptions() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return options_;
}

}  // namespace google::scp::core
