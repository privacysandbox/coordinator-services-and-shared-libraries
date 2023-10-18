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
#include <string>

#include "core/common/uuid/src/uuid.h"

#include "async_context.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {

/// Represents the journal log status.
enum class JournalLogStatus {
  /// Indicates the beginning of a journal log series.
  StartLogGroup = 0,
  /// Indicates a log of a journal log series.
  Log = 1,
  /// Indicates the ending of a journal log series.
  EndLogGroup = 2,
};

/**
 * @brief Represents the journal log request object. This object is used cross
 * code base to store the current memory and state and recover in the case of
 * crash.
 */
struct JournalLogRequest {
  // Log id is a must in the journal log, please do not remove the initializer.
  JournalLogRequest() : log_id(common::Uuid::GenerateUuid()) {}

  /// The component id of the logging request.
  common::Uuid component_id;
  /// The id of the log.
  common::Uuid log_id;
  /// The status of the current journal log.
  JournalLogStatus log_status;
  /// The serialized data the need to be stored.
  std::shared_ptr<BytesBuffer> data;
};

/// Contains journal logs response info.
struct JournalLogResponse {};

/**
 * @brief Represents the journal recovery request object. Any the time of
 * recovery, the application will send a recovery to the journal service to
 * recover the state.
 */
struct JournalRecoverRequest {
  /// The callers can provide the last journal id to process to the journal
  /// service. This will help with partial recovery rather than full recovery,
  /// e.g., checkpoint service. By default the value is set to max uint64 value.
  /// If lower number of journals need to be process, the caller can set this
  /// variable.
  JournalId max_journal_id_to_process = UINT64_MAX;
  /// The maximum number of journals to recover in one call. If not set, all the
  /// journals will be recovered at once.
  JournalId max_number_of_journals_to_process = UINT64_MAX;
  /// Should perform Recovery if there is only a checkpoint to be
  /// recovered in the stream but no journals to be recovered.
  bool should_perform_recovery_with_only_checkpoint_in_stream = true;
};

/// Represents journal recovery response object.
struct JournalRecoverResponse {
  /// The id of the last processed journal log.
  JournalId last_processed_journal_id = 0;
};

/**
 * @brief OnLogRecovered callback
 *
 * @param buffer The buffer containing the log that is being recovered and
 * replayed.
 * @param id ID of the activity for debug logs.
 */
using OnLogRecoveredCallback = std::function<ExecutionResult(
    const std::shared_ptr<BytesBuffer>&, const common::Uuid&)>;

/**
 * @brief JournalService provides write ahead logging functionality. The clients
 * of the this service will be able to keep state, recover state for their
 * operations in the case of crash.
 */
class JournalServiceInterface : public ServiceInterface {
 public:
  virtual ~JournalServiceInterface() = default;

  /**
   * @brief Keeps track of components changes and stores them into streams of
   * logs for recovery in a case of crash.
   *
   * @param log_context The context of the journaling log operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult Log(
      AsyncContext<JournalLogRequest, JournalLogResponse>&
          log_context) noexcept = 0;

  /**
   * @brief Replays all the logs and brings up the instance to its previous
   * state.
   *
   * @param recover_context The context of the journaling recovery operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult Recover(
      AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
          recover_context) noexcept = 0;
  /**
   * @brief Every component that wants to store logs must subscribe to the
   * service by calling SubscribeForRecovery. This will ensure that if the
   * journal service has some pending recovery logs, it will be applied
   * properly.
   *
   * @param component_id The component id, this must be unique between
   * components.
   * @param callback The callback in the case of existence of a recovery log.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult SubscribeForRecovery(
      const common::Uuid& component_id,
      OnLogRecoveredCallback callback) noexcept = 0;

  /**
   * @brief During the shutdown, all the components must unsubscribe from the
   * recovery operation to ensure clean transition to the shut down state.
   *
   * @param component_id The component id, this must be unique between
   * components.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult UnsubscribeForRecovery(
      const common::Uuid& component_id) noexcept = 0;

  /**
   * @brief Returns the last persisted journal id by the current journal
   * service.
   *
   * @param journal_id The journal id to be set.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult GetLastPersistedJournalId(
      JournalId& journal_id) noexcept = 0;

  /**
   * @brief Run the recovery metrics without running the journal service
   * component. Since Recover() method maybe invoked even before the
   * JournalService is Run(), this method is provided as a way to Run() any
   * recovery related metrics before the component is Run().
   *
   */
  virtual ExecutionResult RunRecoveryMetrics() noexcept = 0;

  /**
   * @brief Stop recovery metrics.
   */
  virtual ExecutionResult StopRecoveryMetrics() noexcept = 0;
};
}  // namespace google::scp::core
