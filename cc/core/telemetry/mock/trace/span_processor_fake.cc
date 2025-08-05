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

#include "cc/core/telemetry/mock/trace/span_processor_fake.h"

#include <memory>
#include <utility>

#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/telemetry/mock/trace/error_codes.h"

namespace privacy_sandbox::pbs_common {

inline constexpr absl::string_view kTraceRouterMock = "TraceRouterFake";

SpanProcessorFake::SpanProcessorFake(
    std::shared_ptr<opentelemetry::sdk::trace::SpanExporter> exporter) noexcept
    : exporter_(std::move(exporter)) {}

std::unique_ptr<opentelemetry::sdk::trace::Recordable>
SpanProcessorFake::MakeRecordable() noexcept {
  return exporter_->MakeRecordable();
}

void SpanProcessorFake::OnStart(
    opentelemetry::sdk::trace::Recordable& span,
    const opentelemetry::trace::SpanContext& parent_context) noexcept {
  // No-op
}

void SpanProcessorFake::OnEnd(
    std::unique_ptr<opentelemetry::sdk::trace::Recordable>&& span) noexcept {
  opentelemetry::nostd::span<
      std::unique_ptr<opentelemetry::sdk::trace::Recordable>>
      batch(&span, 1);
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  if (exporter_->Export(batch) ==
      opentelemetry::sdk::common::ExportResult::kFailure) {
    auto execution_result =
        FailureExecutionResult(SC_SPAN_PROCESSOR_COULD_NOT_EXPORT_DATA);
    SCP_ERROR(kTraceRouterMock, kZeroUuid, execution_result,
              "[Trace Router Mock] Could not force flush the data");
  }
}

bool SpanProcessorFake::ForceFlush(std::chrono::microseconds timeout) noexcept {
  if (exporter_ != nullptr) {
    return exporter_->ForceFlush(timeout);
  }
  return true;
}

bool SpanProcessorFake::Shutdown(std::chrono::microseconds timeout) noexcept {
  if (exporter_ != nullptr &&
      !shutdown_latch_.test_and_set(std::memory_order_acquire)) {
    return exporter_->Shutdown(timeout);
  }
  return true;
}

SpanProcessorFake::~SpanProcessorFake() {
  Shutdown();
}

void SpanProcessorFake::Reset() noexcept {
  shutdown_latch_.clear(std::memory_order_release);
}

}  // namespace privacy_sandbox::pbs_common
