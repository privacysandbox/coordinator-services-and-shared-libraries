// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cc/core/telemetry/src/trace/trace_router.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include <include/gmock/gmock-matchers.h>

#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/telemetry/src/common/telemetry_configuration.h"
#include "exporters/ostream/include/opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/resource/resource.h"

namespace privacy_sandbox::pbs_common {
namespace {

class TraceRouterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter =
        opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();

    auto mock_config_provider_ = std::make_shared<MockConfigProvider>();
    int32_t trace_export_interval = 1000;       // 1 second
    int32_t trace_max_queue_size = 2048;        // Example buffer size
    int32_t trace_max_export_batch_size = 512;  // Example batch size
    double trace_sampling_ratio = 1.0;          // 100% sampling

    // Set trace configuration values in the mock config provider.
    // Export interval every 1s.
    mock_config_provider_->SetInt32(
        std::string(kOtelTraceBatchExportIntervalMsecKey),
        trace_export_interval);

    // Max span buffer size.
    mock_config_provider_->SetInt32(std::string(kOtelTraceMaxSpanBufferKey),
                                    trace_max_queue_size);

    // Max export batch size.
    mock_config_provider_->SetInt32(
        std::string(kOtelTraceMaxExportBatchSizeKey),
        trace_max_export_batch_size);

    // Sampling ratio.
    mock_config_provider_->SetDouble(std::string(kOtelTraceSamplingRatioKey),
                                     trace_sampling_ratio);

    // Create TraceRouter instance with default resource and config provider.
    trace_router_ = std::make_unique<TraceRouter>(
        *mock_config_provider_,
        opentelemetry::sdk::resource::Resource::GetDefault(),
        std::move(exporter));
  }

  std::unique_ptr<TraceRouter> trace_router_;
};

TEST_F(TraceRouterTest, TestCreateTracer) {
  // Get the global trace provider.
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();

  // Retrieve or create a tracer from the provider.
  auto tracer = provider->GetTracer("test_service", "1.0");

  // Check that the tracer is created and is not null.
  EXPECT_THAT(tracer, testing::NotNull());
}

}  // namespace
}  // namespace privacy_sandbox::pbs_common
