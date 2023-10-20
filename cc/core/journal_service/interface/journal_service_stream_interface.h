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

#include <atomic>
#include <memory>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/journal_service_interface.h"
#include "core/journal_service/src/proto/journal_service.pb.h"

namespace google::scp::core::journal_service {

struct JournalStreamReadLogObject {
  /// Timestamp of the log.
  Timestamp timestamp;
  /// Component id
  common::Uuid component_id;
  /// Log id
  common::Uuid log_id;
  /// Status of the log.
  JournalLogStatus log_status;
  /// Retrieved log from the log stream.
  std::shared_ptr<journal_service::JournalLog> journal_log;
  /// Journal ID of the journal where the log originated from.
  /// TODO: (optimization) storing this has a tiny overhead for each log.
  /// This need not be stored for each log and can be normalized across logs,
  /// given we know that the logs are obtained in a sequence from a journal and
  /// this will repeat across logs from a given journal.
  JournalId journal_id;
};

/// Represents the journal stream read request object.
struct JournalStreamReadLogRequest {
  /// The callers can provide the last journal id to process to the journal
  /// service. This will help with partial recovery rather than full recovery,
  /// e.g., checkpoint service. By default the value is set to max uint64 value.
  /// If lower number of journals need to be process, the caller can set this
  /// variable.
  JournalId max_journal_id_to_process = UINT64_MAX;
  /// The maximum number of journals to recover in one call. If not set, all the
  /// journals will be recovered at once. For example, this can be used in
  /// checkpointing to limit the number of journals to merge and create a
  /// checkpoint.
  JournalId max_number_of_journals_to_process = UINT64_MAX;
  /// Should perform log reads if there is just only a checkpoint to be
  /// read but no journals to be read in the log stream.
  bool should_read_stream_when_only_checkpoint_exists = true;
};

/// Represents the journal stream read response object.
struct JournalStreamReadLogResponse {
  /// The logs processed from the journals.
  std::shared_ptr<std::vector<JournalStreamReadLogObject>> read_logs;
};

/// Represents the journal stream append request.
struct JournalStreamAppendLogRequest {
  /// Component id
  common::Uuid component_id;
  /// Log id
  common::Uuid log_id;
  /// Status of the log.
  JournalLogStatus log_status;
  /// Log to be appended to the log stream.
  std::shared_ptr<journal_service::JournalLog> journal_log;
};

/// Represents the journal stream append response.
struct JournalStreamAppendLogResponse {};

/**
 * @brief The input stream will be used to read all the journal files since
 * the last checkpoint and combine and return the logs sequentially to be
 * replayed.
 */
class JournalInputStreamInterface {
 public:
  virtual ~JournalInputStreamInterface() = default;

  /**
   * @brief Reads all of the logs from the logs stream. The caller is expected
   * to keep issuing ReadLog() until a EOS marker is returned in the form of
   * ExecutionResult set to
   * FailureExecutionResult(SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN)
   * either in @return or in the context.result.
   *
   *
   * @param read_log_context The context object of the read log operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult ReadLog(
      AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
          read_log_context) noexcept = 0;

  /**
   * @brief Returns the last journal id.
   */
  virtual JournalId GetLastProcessedJournalId() noexcept = 0;
};

/**
 * @brief The output stream will be used to append logs to the current log
 * stream.
 */
class JournalOutputStreamInterface {
 public:
  virtual ~JournalOutputStreamInterface() = default;

  /**
   * @brief Appends a log object to the current log stream.
   *
   * @param append_log_context The context object of the append log operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult AppendLog(
      AsyncContext<JournalStreamAppendLogRequest,
                   JournalStreamAppendLogResponse>&
          append_log_context) noexcept = 0;

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
   * @brief Flushes all the appended logs to the cloud storage.
   *
   * @return ExecutionResult The execution result of the operation.
   */
  virtual ExecutionResult FlushLogs() noexcept = 0;
};
}  // namespace google::scp::core::journal_service
