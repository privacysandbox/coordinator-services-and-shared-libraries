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

#include "journal_output_stream.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "core/blob_storage_provider/src/common/error_codes.h"
#include "core/common/sized_or_timed_bytes_buffer/src/sized_or_timed_bytes_buffer.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/metrics_def.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/journal_utils.h"
#include "google/protobuf/any.pb.h"

using google::scp::core::common::SizedOrTimedBytesBuffer;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalStreamAppendLogRequest;
using google::scp::core::journal_service::JournalStreamAppendLogResponse;
using google::scp::core::journal_service::JournalUtils;
using google::scp::cpio::AggregateMetricInterface;
using google::scp::cpio::MetricClientInterface;
using std::atomic;
using std::bind;
using std::list;
using std::make_shared;
using std::move;
using std::set;
using std::shared_ptr;
using std::sort;
using std::string;
using std::vector;
using std::chrono::milliseconds;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

static constexpr char kJournalOutputStream[] = "JournalOutputStream";

namespace google::scp::core {
JournalOutputStream::JournalOutputStream(
    const shared_ptr<string>& bucket_name,
    const shared_ptr<string>& partition_name,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<BlobStorageClientInterface>& blob_storage_provider_client,
    const shared_ptr<AggregateMetricInterface>& journal_output_count_metric)
    : current_journal_id_(kInvalidJournalId),
      bucket_name_(bucket_name),
      partition_name_(partition_name),
      async_executor_(async_executor),
      blob_storage_provider_client_(blob_storage_provider_client),
      journal_output_count_metric_(journal_output_count_metric),
      last_persisted_journal_id_(kInvalidJournalId),
      pending_logs_(0),
      logs_queue_(INT32_MAX),
      activity_id_(Uuid::GenerateUuid()) {
  // Output activity for log correlation in debugging purposes.
  SCP_DEBUG(kJournalBlobNamePrefix, activity_id_,
            "JournalOutputStream created.");
}

ExecutionResult JournalOutputStream::CreateNewBuffer() noexcept {
  // The journal id needs to be monotonic counter even across restarts, using
  // unique wall clock timestamp for this.
  current_journal_id_ =
      TimeProvider::GetUniqueWallTimestampInNanoseconds().count();

  /// This call will only happen at the creation time of the journal stream
  /// and will not fail.
  shared_ptr<bool> processed;
  auto pair = make_pair(current_journal_id_, make_shared<bool>(false));
  return journals_to_persist_.Insert(pair, processed);
}

ExecutionResult JournalOutputStream::AppendLog(
    AsyncContext<journal_service::JournalStreamAppendLogRequest,
                 journal_service::JournalStreamAppendLogResponse>&
        write_journal_input_stream_context) noexcept {
  auto execution_result =
      logs_queue_.TryEnqueue(write_journal_input_stream_context);
  if (execution_result.Successful()) {
    pending_logs_++;
  }

  return execution_result;
}

ExecutionResult JournalOutputStream::GetLastPersistedJournalId(
    JournalId& journal_id) noexcept {
  journal_id = kInvalidJournalId;
  vector<JournalId> journal_ids;
  auto execution_result = journals_to_persist_.Keys(journal_ids);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  sort(journal_ids.begin(), journal_ids.end());
  for (size_t i = 0; i < journal_ids.size(); ++i) {
    shared_ptr<bool> processed;
    execution_result = journals_to_persist_.Find(journal_ids[i], processed);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    if (!*processed) {
      break;
    }

    journal_id = journal_ids[i];
    execution_result = journals_to_persist_.Erase(journal_id);
    if (!execution_result.Successful()) {
      break;
    }
  }

  if (journal_id == kInvalidJournalId) {
    journal_id = last_persisted_journal_id_;
  }

  if (journal_id == kInvalidJournalId) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_NO_NEW_JOURNAL_ID_AVAILABLE);
  }

  last_persisted_journal_id_ = journal_id;
  return SuccessExecutionResult();
}

size_t JournalOutputStream::GetSerializedLogByteSize(
    AsyncContext<journal_service::JournalStreamAppendLogRequest,
                 journal_service::JournalStreamAppendLogResponse>&
        journal_stream_append_log_context) noexcept {
  return (
      journal_stream_append_log_context.request->journal_log->ByteSizeLong() +
      sizeof(uint64_t) + kLogHeaderByteLength);
}

ExecutionResult JournalOutputStream::SerializeLog(
    AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>&
        journal_stream_append_log_context,
    BytesBuffer& bytes_buffer, size_t& bytes_serialized) noexcept {
  size_t current_buffer_offset = bytes_buffer.length;
  size_t current_bytes_serialized = 0;
  bytes_serialized = 0;

  // The clock value needs to be a wall-clock based timestamp since it will be
  // serialized as part of the log. Since we use this timestamp for ordering
  // logs, we should obtain a unique timestamp.
  Timestamp current_time =
      TimeProvider::GetUniqueWallTimestampInNanoseconds().count();

  auto execution_result = JournalSerialization::SerializeLogHeader(
      bytes_buffer, current_buffer_offset, current_time,
      journal_stream_append_log_context.request->log_status,
      journal_stream_append_log_context.request->component_id,
      journal_stream_append_log_context.request->log_id,
      current_bytes_serialized);

  if (!execution_result.Successful()) {
    return execution_result;
  }

  current_buffer_offset += current_bytes_serialized;
  bytes_serialized += current_bytes_serialized;
  current_bytes_serialized = 0;

  execution_result = JournalSerialization::SerializeJournalLog(
      bytes_buffer, current_buffer_offset,
      *journal_stream_append_log_context.request->journal_log,
      current_bytes_serialized);

  if (!execution_result.Successful()) {
    return execution_result;
  }

  bytes_serialized += current_bytes_serialized;
  return SuccessExecutionResult();
}

ExecutionResult JournalOutputStream::WriteJournalBlob(
    BytesBuffer& bytes_buffer, JournalId journal_id,
    std::function<void(ExecutionResult&)> callback) noexcept {
  SCP_DEBUG(
      kJournalOutputStream, activity_id_,
      "Writing journal blob of byte count: '%llu' for batch with ID '%llu'",
      bytes_buffer.bytes->size(), journal_id);

  if (bytes_buffer.length == 0) {
    auto execution_result = SuccessExecutionResult();
    callback(execution_result);
    return execution_result;
  }

  PutBlobRequest put_blob_request;
  put_blob_request.bucket_name = bucket_name_;
  put_blob_request.buffer = make_shared<BytesBuffer>(move(bytes_buffer));
  auto execution_result = JournalUtils::CreateJournalBlobName(
      partition_name_, journal_id, put_blob_request.blob_name);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_DEBUG(kJournalOutputStream, activity_id_,
            "Putting blob for batch with ID '%llu'", journal_id);

  journal_output_count_metric_->Increment(
      kMetricEventJournalOutputCountWriteJournalScheduledCount);

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context(
      make_shared<PutBlobRequest>(put_blob_request),
      bind(&JournalOutputStream::OnWriteJournalBlobCallback, this, journal_id,
           callback, _1),
      activity_id_, activity_id_);

  return blob_storage_provider_client_->PutBlob(put_blob_context);
}

void JournalOutputStream::OnWriteJournalBlobCallback(
    JournalId journal_id, std::function<void(ExecutionResult&)> callback,
    AsyncContext<PutBlobRequest, PutBlobResponse>& context) noexcept {
  // Even if the context's result is not Successful, we still consider this
  // journal ID as processed i.e. persisted. We do this because failure in
  // context doesn't always mean that journal was not persisted in the
  // journal store, so we assume that it may have been persisted but let
  // callers know that there was an error in persisting, so they will retry
  // their appends (log duplication would happen). If the journal was actually
  // persisted, it will be read by checkpointing service.
  shared_ptr<bool> processed;
  if (journals_to_persist_.Find(journal_id, processed) !=
      SuccessExecutionResult()) {
    SCP_CRITICAL_CONTEXT(
        kJournalOutputStream, context,
        FailureExecutionResult(
            errors::SC_JOURNAL_SERVICE_OUTPUT_STREAM_JOURNAL_STATE_NOT_FOUND),
        "The journal id [%llu] is not found in the "
        "journal_ids_to_persist_status map",
        journal_id);
  } else {
    SCP_DEBUG_CONTEXT(kJournalOutputStream, context,
                      "Journal blob for batch with ID: '%llu' is written",
                      journal_id);
    *processed = true;
  }

  // TODO: Add a quick (local) retry mechanism here for flush failures instead
  // of letting callers know about the failure and asking them to re-append
  // the same logs again.
  if (!context.result.Successful()) {
    // PutBlob was not successful, ensure that we have a trace for this. We
    // let callers (log appenders) retry their appends.
    SCP_ERROR_CONTEXT(kJournalOutputStream, context, context.result,
                      "Failure in persisting journal [%llu] due to an error in "
                      "PutBlob.",
                      journal_id);
    journal_output_count_metric_->Increment(
        kMetricEventJournalOutputCountWriteJournalFailureCount);
  } else {
    journal_output_count_metric_->Increment(
        kMetricEventJournalOutputCountWriteJournalSuccessCount);
  }

  callback(context.result);
}

void JournalOutputStream::WriteBatch(
    const shared_ptr<list<AsyncContext<JournalStreamAppendLogRequest,
                                       JournalStreamAppendLogResponse>>>&
        flush_batch,
    JournalId journal_id) noexcept {
  SCP_DEBUG(kJournalOutputStream, activity_id_,
            "Writing a batch with ID '%llu' of count: '%llu'", journal_id,
            flush_batch->size());

  size_t total_size_needed = 0;
  for (auto it = flush_batch->begin(); it != flush_batch->end(); ++it) {
    total_size_needed += GetSerializedLogByteSize(*it);
  }

  auto buffer = make_shared<BytesBuffer>(total_size_needed);
  size_t total_bytes_serialized = 0;
  for (auto it = flush_batch->begin(); it != flush_batch->end(); ++it) {
    size_t local_total_bytes_serialized = 0;
    auto execution_result =
        SerializeLog(*it, *buffer, local_total_bytes_serialized);
    if (!execution_result.Successful()) {
      NotifyBatch(flush_batch, execution_result);
      return;
    }

    total_bytes_serialized += local_total_bytes_serialized;
    buffer->length += local_total_bytes_serialized;
  }

  if (total_size_needed != total_bytes_serialized) {
    auto execution_result = FailureExecutionResult(
        core::errors::SC_JOURNAL_SERVICE_CORRUPTED_BATCH_OF_LOGS);
    NotifyBatch(flush_batch, execution_result);
    return;
  }

  auto execution_result = WriteJournalBlob(
      *buffer, journal_id,
      bind(&JournalOutputStream::NotifyBatch, this, flush_batch, _1));
  if (!execution_result.Successful()) {
    NotifyBatch(flush_batch, execution_result);
  }
}

void JournalOutputStream::NotifyBatch(
    const shared_ptr<list<AsyncContext<JournalStreamAppendLogRequest,
                                       JournalStreamAppendLogResponse>>>&
        flush_batch,
    ExecutionResult& execution_result) noexcept {
  if (!execution_result.Successful()) {
    SCP_ERROR(kJournalOutputStream, activity_id_, execution_result,
              "Failed to flush logs");
  }

  SCP_DEBUG(kJournalOutputStream, activity_id_,
            "Notifying callers, finishing append contexts in the batch of "
            "size: '%llu'",
            flush_batch->size());

  for (auto it = flush_batch->begin(); it != flush_batch->end(); ++it) {
    it->result = execution_result;
    it->Finish();
  }
}

ExecutionResult JournalOutputStream::FlushLogs() noexcept {
  // One flush at a time
  create_batch_of_logs_mutex_.lock();
  size_t batch_size = pending_logs_.load();
  if (batch_size == 0) {
    create_batch_of_logs_mutex_.unlock();
    return SuccessExecutionResult();
  }

  auto execution_result = CreateNewBuffer();
  if (!execution_result.Successful()) {
    create_batch_of_logs_mutex_.unlock();
    return execution_result;
  }

  auto current_journal_id = current_journal_id_;
  auto batch_logs =
      make_shared<list<AsyncContext<JournalStreamAppendLogRequest,
                                    JournalStreamAppendLogResponse>>>();
  for (size_t i = 0; i < batch_size;) {
    AsyncContext<JournalStreamAppendLogRequest, JournalStreamAppendLogResponse>
        context;
    if (!logs_queue_.TryDequeue(context).Successful()) {
      // Cannot increment i until we get all the messages.
      continue;
    }

    batch_logs->push_back(context);
    ++i;
  }

  pending_logs_ -= batch_size;

  SCP_DEBUG(kJournalOutputStream, activity_id_,
            "Created a batch of logs with ID: '%llu' of size: '%llu'. "
            "Remaining logs in the queue: '%llu'",
            current_journal_id, batch_logs->size(), pending_logs_.load());

  create_batch_of_logs_mutex_.unlock();

  execution_result = async_executor_->Schedule(
      [this, batch_logs, current_journal_id]() {
        WriteBatch(batch_logs, current_journal_id);
      },
      AsyncPriority::Urgent);

  if (!execution_result.Successful()) {
    NotifyBatch(batch_logs, execution_result);
  }

  return execution_result;
}

}  // namespace google::scp::core
