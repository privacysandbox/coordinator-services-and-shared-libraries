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
#include <string>

#include <core/config_provider/mock/mock_config_provider.h>

#include "core/telemetry/src/common/telemetry_configuration.h"
#include "include/gtest/gtest.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace google::scp::core {
namespace {
class OtlpGrpcAuthedExporterMetricTest : public testing::Test {
 protected:
  inline static constexpr absl::string_view kDefaultEndpoint =
      "localhost:45454";

  void SetUp() override {
    mock_config_provider_ =
        std::make_shared<config_provider::mock::MockConfigProvider>();
    std::int32_t metric_export_interval = 1000;
    std::int32_t metric_export_timeout = 500;
    std::string service_account = "service_account";
    std::string audience = "audience";
    std::string cred_config_path = "";

    mock_config_provider_->SetInt32(
        std::string(kOtelMetricExportIntervalMsecKey),
        metric_export_interval);  // exporting every 1s
    mock_config_provider_->SetInt32(
        std::string(kOtelMetricExportTimeoutMsecKey),
        metric_export_timeout);  // timeout every .5s
    mock_config_provider_->Set(std::string(kOtelServiceAccountKey),
                               service_account);
    mock_config_provider_->Set(std::string(kOtelAudienceKey), audience);
    mock_config_provider_->Set(std::string(kOtelCredConfigKey),
                               cred_config_path);

    grpc_id_token_authenticator_ = std::make_unique<GrpcIdTokenAuthenticator>();
  }

  std::unique_ptr<OtlpGrpcAuthedMetricExporter> CreateExporter(
      const std::string& endpoint) {
    opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions options;
    options.aggregation_temporality = opentelemetry::exporter::otlp::
        PreferredAggregationTemporality::kUnspecified;
    options.endpoint = endpoint;
    return std::make_unique<OtlpGrpcAuthedMetricExporter>(
        options, std::move(grpc_id_token_authenticator_));
  }

  std::shared_ptr<config_provider::mock::MockConfigProvider>
      mock_config_provider_;
  std::unique_ptr<GrpcIdTokenAuthenticator> grpc_id_token_authenticator_;
};

TEST_F(OtlpGrpcAuthedExporterMetricTest, ConfigTest) {
  opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions opts;
  opts.endpoint = kDefaultEndpoint;
  std::unique_ptr<OtlpGrpcAuthedMetricExporter> exporter =
      CreateExporter(std::string(kDefaultEndpoint));
  EXPECT_EQ(exporter->GetOptions().endpoint, "localhost:45454");
}

TEST_F(OtlpGrpcAuthedExporterMetricTest,
       ExportShouldReturnFailureWhenExporterIsShutdown) {
  std::unique_ptr<OtlpGrpcAuthedMetricExporter> exporter =
      CreateExporter(std::string(kDefaultEndpoint));
  exporter->Shutdown(std::chrono::microseconds::max());
  opentelemetry::sdk::metrics::ResourceMetrics data;

  opentelemetry::sdk::common::ExportResult result = exporter->Export(data);
  EXPECT_EQ(result, opentelemetry::sdk::common::ExportResult::kFailure);
}

TEST_F(OtlpGrpcAuthedExporterMetricTest, ForceFlushShouldAlwaysReturnTrue) {
  std::unique_ptr<OtlpGrpcAuthedMetricExporter> exporter =
      CreateExporter(std::string(kDefaultEndpoint));
  EXPECT_TRUE(exporter->ForceFlush(std::chrono::microseconds::max()));
}
}  // namespace
}  // namespace google::scp::core
