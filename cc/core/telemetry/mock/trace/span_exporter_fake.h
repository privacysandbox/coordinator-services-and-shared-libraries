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
#ifndef CC_CORE_TELEMETRY_MOCK_TRACE_SPAN_EXPORTER_FAKE_H_
#define CC_CORE_TELEMETRY_MOCK_TRACE_SPAN_EXPORTER_FAKE_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/span_data.h"

namespace privacy_sandbox::pbs_common {

/**
 * @brief A mock implementation of the SpanExporter interface for testing
 * purposes.
 *
 * This class simulates the behavior of an OpenTelemetry span exporter. It
 * collects and stores spans in memory instead of sending them to an external
 * telemetry service. It allows testing of span export logic in a controlled
 * environment.
 */
class SpanExporterFake : public opentelemetry::sdk::trace::SpanExporter {
 public:
  SpanExporterFake() noexcept;

  /**
   * @brief Creates a new span recordable.
   *
   * This method is used to create instances of Recordable, which represent
   * span data that can be exported. The created recordable is stored in memory
   * for testing.
   *
   * @return A unique pointer to a new Recordable object.
   */
  std::unique_ptr<opentelemetry::sdk::trace::Recordable>
  MakeRecordable() noexcept override;

  /**
   * @brief Collects spans for export.
   *
   * This method stores spans in an internal container, simulating the export
   * process. The collected spans can then be accessed for verification during
   * testing.
   *
   * @param spans A span of unique pointers to Recordable objects, representing
   * the spans to be exported.
   * @return ExportResult indicating success or failure of the export operation.
   */
  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::nostd::span<
          std::unique_ptr<opentelemetry::sdk::trace::Recordable>>&
          spans) noexcept override;

  /**
   * @brief Forces a flush of spans.
   *
   * Simulates the process of forcefully flushing any buffered spans to the
   * exporter within the specified timeout. In this mock implementation, it
   * simply returns true, as actual flushing is not needed.
   *
   * @param timeout The maximum duration to wait for the flush to complete.
   * Defaults to the maximum possible value.
   * @return Always returns true in this mock implementation.
   */
  bool ForceFlush(std::chrono::microseconds timeout =
                      (std::chrono::microseconds::max)()) noexcept override;

  /**
   * @brief Shuts down the exporter.
   *
   * Sets the internal state to indicate that the exporter is shut down. This
   * method prevents further span exports.
   *
   * @param timeout The maximum duration to wait for the shutdown to complete.
   * Defaults to the maximum possible value.
   * @return True if shutdown completed successfully; otherwise, false.
   */
  bool Shutdown(std::chrono::microseconds timeout =
                    (std::chrono::microseconds::max)()) noexcept override;

  /**
   * @brief Retrieves spans collected for a specific trace ID.
   *
   * Provides access to the spans that have been collected and stored in memory
   * for the given trace ID. The span data remains owned by the exporter class.
   * This method returns a vector of const pointers to the span data
   * objects, allowing access without transferring ownership.
   *
   * @param trace_id The trace ID for which to retrieve the collected spans.
   * @return A vector of const pointer to SpanData objects associated
   * with the specified trace ID. If no spans are found for the trace ID, an
   * empty vector is returned.
   */
  [[nodiscard]] std::vector<const opentelemetry::sdk::trace::SpanData*>
  GetSpansForTraceId(absl::string_view trace_id) const;

  /**
   * @brief Retrieves spans collected for a specific span name.
   *
   * Provides access to the spans that have been collected and stored in memory
   * for the given span name. The span data remains owned by the exporter class.
   * This method returns a vector of const pointers to the span data
   * objects, allowing access without transferring ownership.
   *
   * @param span_name The span name for which to retrieve the collected spans.
   * @return A vector of const pointer to SpanData objects associated
   * with the specified span name. If no spans are found for the span name, an
   * empty vector is returned.
   */
  [[nodiscard]] std::vector<const opentelemetry::sdk::trace::SpanData*>
  GetSpansForSpanName(absl::string_view span_name) const;

  /**
   * @brief Accessor for the collected spans as references mapped by trace ID.
   *
   * Provides access to the internal map of spans as const pointers, ensuring
   * that the exporter retains ownership of the span data while allowing access
   * to verify the collected spans for each trace ID.
   *
   * @return A map of trace IDs and vectors of const pointers to the `SpanData`
   * objects associated with each trace.
   */
  [[nodiscard]] absl::flat_hash_map<
      std::string, std::vector<const opentelemetry::sdk::trace::SpanData*>>
  GetCollectedSpansByTraceId() const;

  /**
   * @brief Accessor for the collected spans as references mapped by span name.
   *
   * Provides access to the internal map of spans as const pointers, ensuring
   * that the exporter retains ownership of the span data while allowing access
   * to verify the collected spans for each span name.
   *
   * @return A map of trace IDs and vectors of const pointers to the `SpanData`
   * objects associated with each span name.
   */
  [[nodiscard]] absl::flat_hash_map<
      std::string, std::vector<const opentelemetry::sdk::trace::SpanData*>>
  GetCollectedSpansBySpanName() const;

 private:
  // Indicates whether the exporter has been shut down.
  std::atomic<bool> is_shutdown_{false};

  // Internal storage of span data to keep the ownership.
  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanData>> span_data_;

  // Internal storage for collected spans. Maps trace IDs to vectors of span
  // data.
  absl::flat_hash_map<std::string,
                      std::vector<const opentelemetry::sdk::trace::SpanData*>>
      collected_spans_;

  // Internal storage for collected spans. Maps span name to vectors of span
  // data.
  absl::flat_hash_map<std::string,
                      std::vector<const opentelemetry::sdk::trace::SpanData*>>
      collected_spans_by_name_;
};

}  // namespace privacy_sandbox::pbs_common
#endif  // CC_CORE_TELEMETRY_MOCK_TRACE_SPAN_EXPORTER_FAKE_H_
