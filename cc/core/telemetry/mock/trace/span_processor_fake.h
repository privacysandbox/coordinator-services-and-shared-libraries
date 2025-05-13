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
#ifndef CC_CORE_TELEMETRY_MOCK_TRACE_SPAN_PROCESSOR_FAKE_H_
#define CC_CORE_TELEMETRY_MOCK_TRACE_SPAN_PROCESSOR_FAKE_H_

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "opentelemetry/common/spin_lock_mutex.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "opentelemetry/trace/span_context.h"

namespace privacy_sandbox::pbs_common {

/**
 * @brief A mock span processor for testing purposes.
 * This mock span processor provides simple implementations for span
 * processing methods for use in testing telemetry components.
 */
class SpanProcessorFake : public opentelemetry::sdk::trace::SpanProcessor {
 public:
  /**
   * @brief Constructs the mock span processor.
   * @param exporter The exporter used by the span processor.
   */
  explicit SpanProcessorFake(
      std::shared_ptr<opentelemetry::sdk::trace::SpanExporter>
          exporter) noexcept;

  /**
   * @brief Creates a recordable for spans.
   * @return A unique pointer to a Recordable instance.
   */
  std::unique_ptr<opentelemetry::sdk::trace::Recordable>
  MakeRecordable() noexcept override;

  /**
   * @brief Called when a span starts.
   * This mock implementation does nothing.
   */
  void OnStart(opentelemetry::sdk::trace::Recordable& span,
               const opentelemetry::trace::SpanContext& parent_context) noexcept
      override;

  /**
   * @brief Called when a span ends.
   * This mock implementation processes the span and stores it.
   */
  void OnEnd(std::unique_ptr<opentelemetry::sdk::trace::Recordable>&&
                 span) noexcept override;

  /**
   * @brief Forces the processor to flush any buffered spans.
   * @param timeout The maximum duration to wait for the flush.
   * @return Always returns true in this mock implementation.
   */
  bool ForceFlush(std::chrono::microseconds timeout =
                      (std::chrono::microseconds::max)()) noexcept override;

  /**
   * @brief Shuts down the processor.
   * @param timeout The maximum duration to wait for shutdown.
   * @return Always returns true in this mock implementation.
   */
  bool Shutdown(std::chrono::microseconds timeout =
                    (std::chrono::microseconds::max)()) noexcept override;

  /**
   * @brief Destructor for the mock span processor.
   */
  ~SpanProcessorFake() override;

  /**
   * @brief Resets the mock span processor for testing purposes.
   */
  void Reset() noexcept;

 private:
  // The span exporter used to export collected span data. It handles sending
  // span data to an external telemetry service.
  std::shared_ptr<opentelemetry::sdk::trace::SpanExporter> exporter_;

  // A mutex to protect access to shared resources, ensuring thread safety when
  // modifying collected spans.
  opentelemetry::common::SpinLockMutex lock_;

  // Atomic flag used as a latch to coordinate the shutdown process, ensuring it
  // happens only once.
  std::atomic_flag shutdown_latch_ = ATOMIC_FLAG_INIT;
};

}  // namespace privacy_sandbox::pbs_common
#endif  // CC_CORE_TELEMETRY_MOCK_TRACE_SPAN_PROCESSOR_FAKE_H_
