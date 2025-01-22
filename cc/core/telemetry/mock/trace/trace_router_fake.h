// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef CC_CORE_TELEMETRY_MOCK_TRACE_TRACE_ROUTER_FAKE_H_
#define CC_CORE_TELEMETRY_MOCK_TRACE_TRACE_ROUTER_FAKE_H_

#include <memory>
#include <string>
#include <vector>

#include "cc/core/interface/errors.h"
#include "cc/core/telemetry/mock/trace/span_exporter_fake.h"
#include "cc/core/telemetry/src/trace/trace_router.h"
#include "cc/public/core/interface/execution_result.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/trace/tracer.h"

namespace google::scp::core {

/**
 * @brief A mock TraceRouter for testing purposes.
 * This mock TraceRouter class provides basic implementations for trace
 * operations and is used in unit tests to simulate interactions with the
 * TraceRouter class.
 */
class TraceRouterFake : public TraceRouter {
 public:
  /**
   * @brief Constructs the mock TraceRouter.
   */
  TraceRouterFake();

  /**
   * @brief Retrieves the in-memory SpanExporterFake.
   * @return Reference to the in-memory SpanExporterFake.
   */
  [[nodiscard]] SpanExporterFake& GetSpanExporter() const;

  /**
   * @brief Accessor for the exported traces as const pointer.
   *
   * Returns a map of trace IDs to vectors of const pointer to `SpanData`
   * objects stored in the exporter. SpanExporterFake class has the ownership of
   * SpanData.
   *
   */
  [[nodiscard]] absl::flat_hash_map<
      std::string, std::vector<const opentelemetry::sdk::trace::SpanData*>>
  GetExportedTraces() const;

  /**
   * @brief Accessor for spans associated with a specific trace ID.
   *
   * Returns a vector of const pointer to `SpanData` objects associated
   * with the specified trace ID. SpanExporterFake class has the ownership of
   * SpanData.
   *
   * @param trace_id The ID of the trace for which spans are requested.
   */
  [[nodiscard]] std::vector<const opentelemetry::sdk::trace::SpanData*>
  GetSpansForTrace(absl::string_view trace_id) const;

  /**
   * @brief Accessor for the exported spans as const pointers.
   *
   * Returns a map of span names to vectors of const pointer to `SpanData`
   * objects stored in the exporter. SpanExporterFake class has the ownership of
   * SpanData.
   *
   */
  [[nodiscard]] absl::flat_hash_map<
      std::string, std::vector<const opentelemetry::sdk::trace::SpanData*>>
  GetExportedSpansBySpanName() const;

  /**
   * @brief Accessor for spans associated with a specific span name.
   *
   * Returns a vector of const pointer to `SpanData` objects associated
   * with the specified span name. SpanExporterFake class has the ownership of
   * SpanData.
   *
   * @param span_name The span name for which spans are requested.
   */
  [[nodiscard]] std::vector<const opentelemetry::sdk::trace::SpanData*>
  GetSpansForSpanName(absl::string_view span_name) const;

 private:
  std::shared_ptr<SpanExporterFake> span_exporter_;
};
}  // namespace google::scp::core
#endif  // CC_CORE_TELEMETRY_MOCK_TRACE_TRACE_ROUTER_FAKE_H_
