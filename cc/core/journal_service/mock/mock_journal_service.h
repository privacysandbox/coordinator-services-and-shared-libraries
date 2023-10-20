/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <functional>
#include <memory>

#include "core/interface/journal_service_interface.h"

namespace google::scp::core::journal_service::mock {
class MockJournalService : public JournalServiceInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult GetLastPersistedJournalId(
      JournalId& journal_id) noexcept override {
    journal_id = 0;
    return SuccessExecutionResult();
  }

  ExecutionResult Log(AsyncContext<JournalLogRequest, JournalLogResponse>&
                          log_context) noexcept override {
    if (log_mock) {
      return log_mock(log_context);
    }

    log_context.result = SuccessExecutionResult();
    log_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult Recover(
      AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
          recover_context) noexcept override {
    if (recover_mock) {
      return recover_mock(recover_context);
    }
    recover_context.result = SuccessExecutionResult();
    recover_context.Finish();
    return SuccessExecutionResult();
  }

  ExecutionResult SubscribeForRecovery(
      const common::Uuid& component_id,
      OnLogRecoveredCallback callback) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult UnsubscribeForRecovery(
      const common::Uuid& component_id) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult RunRecoveryMetrics() noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult StopRecoveryMetrics() noexcept override {
    return SuccessExecutionResult();
  }

  std::function<ExecutionResult(
      AsyncContext<JournalLogRequest, JournalLogResponse>&)>
      log_mock;

  std::function<ExecutionResult(
      AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&)>
      recover_mock;
};
}  // namespace google::scp::core::journal_service::mock
