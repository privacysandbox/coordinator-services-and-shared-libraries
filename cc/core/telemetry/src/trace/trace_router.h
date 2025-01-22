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

#ifndef CC_CORE_TELEMETRY_SRC_TRACE_TRACE_ROUTER_H_
#define CC_CORE_TELEMETRY_SRC_TRACE_TRACE_ROUTER_H_

#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "cc/core/interface/config_provider_interface.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/tracer.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/tracer.h"

namespace google::scp::core {

/**
 * @brief TraceRouter class for handling OpenTelemetry traces.
 *
 * The TraceRouter class manages an OpenTelemetry TracerProvider
 * and provides access to it for trace operations. It sets up and maintains
 * tracers, processors, and samplers for handling and exporting trace data.
 *
 * The TraceRouter is designed to be instantiated only once per
 * application/service/server. Multiple instances of TraceRouter within the same
 * application may lead to:
 *   - Duplicate trace entries
 *   - Inconsistent or fragmented tracing data
 *   - Increased resource overhead (memory and CPU usage)
 *   - Potential conflicts in tracing configurations
 *
 * To avoid these issues, TraceRouter is initialized in pbs_instance_v3.
 */
class TraceRouter {
 public:
  explicit TraceRouter(
      ConfigProviderInterface& config_provider,
      const opentelemetry::sdk::resource::Resource& resource,
      std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> span_exporter);

 protected:
  TraceRouter() = default;
  /**
   * @brief Sets up the TraceRouter with the provided SpanProcessor and Sampler.
   *
   * This method initializes the trace provider with the provided SpanProcessor
   * and Sampler, and sets it as the global tracer provider.
   *
   * @param resource The resource information for the tracer.
   * @param span_processor A unique pointer to the SpanProcessor.
   * @param span_sampler A unique pointer to the Sampler.
   */
  void SetupTraceRouter(
      const opentelemetry::sdk::resource::Resource& resource,
      std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> span_processor,
      std::unique_ptr<opentelemetry::sdk::trace::Sampler> span_sampler);
};

}  // namespace google::scp::core
#endif  // CC_CORE_TELEMETRY_SRC_TRACE_TRACE_ROUTER_H_
