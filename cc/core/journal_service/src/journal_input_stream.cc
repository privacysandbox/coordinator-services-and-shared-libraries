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

#include "journal_input_stream.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "core/blob_storage_provider/src/common/error_codes.h"
#include "core/interface/type_def.h"
#include "core/journal_service/interface/journal_service_stream_interface.h"
#include "core/journal_service/src/journal_serialization.h"
#include "core/journal_service/src/journal_utils.h"
#include "core/journal_service/src/proto/journal_service.pb.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::common::Uuid;
using google::scp::core::journal_service::CheckpointMetadata;
using google::scp::core::journal_service::JournalLog;
using google::scp::core::journal_service::JournalSerialization;
using google::scp::core::journal_service::JournalStreamReadLogObject;
using google::scp::core::journal_service::JournalStreamReadLogRequest;
using google::scp::core::journal_service::JournalStreamReadLogResponse;
using google::scp::core::journal_service::JournalUtils;
using google::scp::core::journal_service::LastCheckpointMetadata;
using std::atomic;
using std::bind;
using std::list;
using std::make_shared;
using std::max_element;
using std::move;
using std::set;
using std::shared_ptr;
using std::sort;
using std::string;
using std::vector;
using std::placeholders::_1;

// TODO: Use configuration provider to update the following.
static constexpr char kLastCheckpointBlobName[] = "last_checkpoint";
static constexpr char kJournalInputStream[] = "JournalInputStream";

namespace google::scp::core {

/**
 * @brief Calls finish on the context by putting the supplied logs in the
 * response.
 *
 * @param context
 * @param logs
 */
static void FinishContextWithResponse(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        context,
    std::shared_ptr<std::vector<JournalStreamReadLogObject>>&& logs) {
  context.response = make_shared<JournalStreamReadLogResponse>();
  context.response->read_logs = logs;
  FinishContext(SuccessExecutionResult(), context);
}

bool JournalInputStream::IsJournalBuffersLoadedButNotProcessedYet() {
  return !journal_buffers_.empty() &&
         current_buffer_index_ < journal_buffers_.size();
}

ExecutionResult JournalInputStream::ReadLog(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context) noexcept {
  // When enable_batch_read_journals_ is false, ReadLog will perform the
  // following steps to recover logs:
  //
  // 1. Try to read the last checkpoint file.
  //    - If exists go to 3.
  //    - Else go to 2.
  // 2. List all the checkpoint files to identify the last checkpoint file
  // 3. Read all the journal ids after the last checkpoint's last processed
  //    journal id. If no checkpoint file exists, read all journals from
  //    beginning.
  // 4. Return the stream, which includes buffers for the last checkpoint's data
  //    and each of the journals which occur after checkpoint's last processed
  //    journal id.
  //
  // When enable_batch_read_journals_ is true, ReadLog will perform the
  // following steps to recover logs:
  //
  // 1. Check if journal_ids are already loaded in Step 4. If so, go to step 5,
  //    else go to Step 2.
  // 2. Try reading the last_checkpoint file.
  //    - If exists go to 4.
  //    - Else go to 3.
  // 3. List all the checkpoint files to identify the last checkpoint file
  // 4. List all the journal ids after the last checkpoint's last processed
  //    journal id. If no checkpoint file exists, read all journals from
  //    beginning. After this step is completed, all journal_ids are considere
  //    to be loaded into memory.
  // 5. Process a batch of journal_ids.
  //
  //    5.1. If journal_ids is being processed for the first time, or if
  //    previously read journal blobs have been processed, read journal blobs
  //    from persistance storage and store them in memory. The number of journal
  //    blobs to be read is specified by number_of_journals_per_batch_.
  //
  //    5.2. Process journal blobs by deserializing the journal blob to journal
  //    logs and returns the list of journal logs to caller by using the
  //    callback function in AsyncContext. The maximum number of journal logs to
  //    be returned by this step is specified by
  //    number_of_journal_logs_to_return_.

  // TODO: Decouple Loading of journals from Returning journals to caller. Make
  // another API for the JournalInputStreamInterface to Load the stream.

  // If this is the first time calling read, it is required to
  // initialize the logs.
  bool loaded =
      enable_batch_read_journals_ ? journal_ids_loaded_ : journals_loaded_;
  if (!loaded) {
    // Kick start Step 1
    return ReadLastCheckpointBlob(journal_stream_read_log_context);
  }

  if (!enable_batch_read_journals_) {
    auto execution_result =
        ProcessLoadedJournals(journal_stream_read_log_context);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    return SuccessExecutionResult();
  }

  if (IsJournalBuffersLoadedButNotProcessedYet()) {
    // If a list of journal ids has been listed but it is an empty list, and
    // there is something in the journal_buffers_, it means that the only things
    // in the journal_buffers_ must be the checkpoint journal.
    //
    // In this case, only proceed with reading the checkpoint journal blob if
    // the request said so, i.e., when
    // should_read_stream_when_only_checkpoint_exists is set to true.
    if (journal_ids_.empty() && journal_buffers_.size() == 1 &&
        !journal_stream_read_log_context.request
             ->should_read_stream_when_only_checkpoint_exists) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN);
    }
    if (auto execution_result =
            ProcessLoadedJournals(journal_stream_read_log_context);
        !execution_result.Successful()) {
      return execution_result;
    }
    return SuccessExecutionResult();
  }

  // Journal IDs must be loaded at least once before calling ReadJournalBlobs.
  if (!journal_ids_loaded_) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_INPUT_STREAM_INVALID_LISTING);
  }
  if (auto execution_result =
          ReadJournalBlobs(journal_stream_read_log_context, journal_ids_);
      !execution_result.Successful()) {
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult JournalInputStream::ReadLastCheckpointBlob(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context) noexcept {
  GetBlobRequest get_blob_request;
  get_blob_request.bucket_name = bucket_name_;
  auto execution_result = JournalUtils::GetBlobFullPath(
      partition_name_, make_shared<string>(kLastCheckpointBlobName),
      get_blob_request.blob_name);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  SCP_DEBUG_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                    "Reading the last checkpoint blob");

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      make_shared<GetBlobRequest>(move(get_blob_request)),
      bind(&JournalInputStream::OnReadLastCheckpointBlobCallback, this,
           journal_stream_read_log_context, _1),
      journal_stream_read_log_context);

  return blob_storage_provider_client_->GetBlob(get_blob_context);
}

void JournalInputStream::OnReadLastCheckpointBlobCallback(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept {
  bool last_checkpoint_found = true;
  if (!get_blob_context.result.Successful()) {
    if (get_blob_context.result !=
        FailureExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND)) {
      SCP_ERROR_CONTEXT(kJournalInputStream, get_blob_context,
                        get_blob_context.result,
                        "Error reading last_checkpoint blob");
      return FinishContext(get_blob_context.result,
                           journal_stream_read_log_context);
    }
    last_checkpoint_found = false;
  }

  if (!last_checkpoint_found) {
    SCP_DEBUG_CONTEXT(kJournalInputStream, get_blob_context,
                      "The last checkpoint blob was not found, listing from "
                      "the beginning.");

    // Last checkpoint blob not found, list all the available checkpoint
    // blobs.
    shared_ptr<Blob> start_from = nullptr;
    auto execution_result =
        ListCheckpoints(journal_stream_read_log_context, start_from);
    if (!execution_result.Successful()) {
      return FinishContext(execution_result, journal_stream_read_log_context);
    }
    return;
  }

  // Deserialize the last checkpoint metadata.
  LastCheckpointMetadata last_checkpoint_metadata;
  size_t buffer_offset = 0;
  size_t bytes_deserialized = 0;
  auto execution_result =
      JournalSerialization::DeserializeLastCheckpointMetadata(
          *get_blob_context.response->buffer, buffer_offset,
          last_checkpoint_metadata, bytes_deserialized);
  if (!execution_result.Successful()) {
    return FinishContext(execution_result, journal_stream_read_log_context);
  }

  // Set the last checkpoint id
  last_checkpoint_id_ = last_checkpoint_metadata.last_checkpoint_id();
  if (last_checkpoint_id_ == kInvalidJournalId) {
    execution_result = FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_INPUT_STREAM_INVALID_LAST_CHECKPOINT);
    return FinishContext(execution_result, journal_stream_read_log_context);
  }

  // now read the checkpoint blob that the last checkpoint points to
  execution_result =
      ReadCheckpointBlob(journal_stream_read_log_context, last_checkpoint_id_);
  if (!execution_result.Successful()) {
    return FinishContext(execution_result, journal_stream_read_log_context);
  }
}

ExecutionResult JournalInputStream::ReadCheckpointBlob(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    CheckpointId checkpoint_id) noexcept {
  Blob checkpoint_blob;
  checkpoint_blob.bucket_name = bucket_name_;
  auto execution_result = JournalUtils::CreateCheckpointBlobName(
      partition_name_, checkpoint_id, checkpoint_blob.blob_name);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  GetBlobRequest get_blob_request;
  get_blob_request.bucket_name = checkpoint_blob.bucket_name;
  get_blob_request.blob_name = checkpoint_blob.blob_name;

  SCP_DEBUG_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                    "Reading checkpoint blob with blob name: %s.",
                    get_blob_request.blob_name->c_str());

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      make_shared<GetBlobRequest>(move(get_blob_request)),
      bind(&JournalInputStream::OnReadCheckpointBlobCallback, this,
           journal_stream_read_log_context, _1),
      journal_stream_read_log_context);

  return blob_storage_provider_client_->GetBlob(get_blob_context);
}

void JournalInputStream::OnReadCheckpointBlobCallback(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept {
  if (!get_blob_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kJournalInputStream, get_blob_context,
                      get_blob_context.result,
                      "Error reading checkpoint blob with blob name: %s.",
                      get_blob_context.request->blob_name->c_str());
    return FinishContext(get_blob_context.result,
                         journal_stream_read_log_context);
  }

  CheckpointMetadata checkpoint_metadata;
  size_t buffer_offset = 0;
  size_t bytes_deserialized = 0;
  auto execution_result = JournalSerialization::DeserializeCheckpointMetadata(
      *get_blob_context.response->buffer, buffer_offset, checkpoint_metadata,
      bytes_deserialized);
  if (!execution_result.Successful()) {
    return FinishContext(execution_result, journal_stream_read_log_context);
  }

  last_processed_journal_id_ = checkpoint_metadata.last_processed_journal_id();
  SCP_INFO_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                   "The last journal id read from the last checkpoint metadata "
                   "is: %llu. Listing all journals after this.",
                   last_processed_journal_id_);

  // Checkpoint metadata is present at the end of the buffer and is not
  // necessary anymore.
  auto prefix_length_to_consume =
      get_blob_context.response->buffer->length - bytes_deserialized;
  BytesBuffer checkpoint_buffer(get_blob_context.response->buffer,
                                prefix_length_to_consume);

  // Checkpoint data needs to be processed as well.
  // Checkpoint buffer is stored at the index '0' in journal_buffers_
  journal_buffers_.push_back(checkpoint_buffer);

  shared_ptr<Blob> start_from = make_shared<Blob>();
  JournalUtils::CreateJournalBlobName(
      partition_name_, last_processed_journal_id_, start_from->blob_name);
  start_from->bucket_name = bucket_name_;
  execution_result = ListJournals(journal_stream_read_log_context, start_from);

  if (!execution_result.Successful()) {
    return FinishContext(execution_result, journal_stream_read_log_context);
  }
}

ExecutionResult JournalInputStream::ListCheckpoints(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    std::shared_ptr<Blob>& start_from) noexcept {
  ListBlobsRequest list_blobs_request;
  list_blobs_request.bucket_name = bucket_name_;

  auto execution_result = JournalUtils::GetBlobFullPath(
      partition_name_, make_shared<string>(kCheckpointBlobNamePrefix),
      list_blobs_request.blob_name);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (start_from) {
    list_blobs_request.marker = start_from->blob_name;
    SCP_DEBUG_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                      "Listing all checkpoint blobs from %s",
                      start_from->blob_name->c_str());
  } else {
    SCP_DEBUG_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                      "Listing all checkpoint blobs");
  }

  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context(
      make_shared<ListBlobsRequest>(move(list_blobs_request)),
      bind(&JournalInputStream::OnListCheckpointsCallback, this,
           journal_stream_read_log_context, _1),
      journal_stream_read_log_context);

  return blob_storage_provider_client_->ListBlobs(list_blobs_context);
}

void JournalInputStream::OnListCheckpointsCallback(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    AsyncContext<ListBlobsRequest, ListBlobsResponse>&
        list_blobs_context) noexcept {
  if (!list_blobs_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kJournalInputStream, list_blobs_context,
                      list_blobs_context.result, "Failed to list checkpoints");
    return FinishContext(list_blobs_context.result,
                         journal_stream_read_log_context);
  }

  SCP_INFO_CONTEXT(kJournalInputStream, list_blobs_context,
                   "Listed total %llu checkpoint blobs",
                   list_blobs_context.response->blobs->size());

  // Get the latest checkpoint file
  for (const auto& checkpoint_blob : *list_blobs_context.response->blobs) {
    CheckpointId checkpoint_id = kInvalidJournalId;
    auto execution_result = JournalUtils::ExtractCheckpointId(
        partition_name_, checkpoint_blob.blob_name, checkpoint_id);
    if (!execution_result.Successful()) {
      return FinishContext(execution_result, journal_stream_read_log_context);
    }

    if (checkpoint_id > last_checkpoint_id_) {
      last_checkpoint_id_ = checkpoint_id;
    }
  }

  // If there are more checkpoints remaining
  if (list_blobs_context.response->next_marker &&
      !list_blobs_context.response->next_marker->blob_name->empty()) {
    auto execution_result =
        ListCheckpoints(journal_stream_read_log_context,
                        list_blobs_context.response->next_marker);
    if (!execution_result.Successful()) {
      return FinishContext(execution_result, journal_stream_read_log_context);
    }
    return;
  }

  // Do one more listing to be sure that there are no more remaining.
  if (!list_blobs_context.response->blobs->empty()) {
    auto next_marker =
        make_shared<Blob>(list_blobs_context.response->blobs->back());
    auto execution_result =
        ListCheckpoints(journal_stream_read_log_context, next_marker);
    if (!execution_result.Successful()) {
      return FinishContext(execution_result, journal_stream_read_log_context);
    }
    return;
  }

  // If there are no checkpoints, start listing journals from the beginning.
  if (last_checkpoint_id_ == kInvalidJournalId) {
    shared_ptr<Blob> start_from = nullptr;
    auto execution_result =
        ListJournals(journal_stream_read_log_context, start_from);
    if (!execution_result.Successful()) {
      return FinishContext(execution_result, journal_stream_read_log_context);
    }
    return;
  }

  auto execution_result =
      ReadCheckpointBlob(journal_stream_read_log_context, last_checkpoint_id_);

  if (!execution_result.Successful()) {
    return FinishContext(execution_result, journal_stream_read_log_context);
  }
}

ExecutionResult JournalInputStream::ListJournals(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    std::shared_ptr<Blob>& start_from) noexcept {
  ListBlobsRequest list_blobs_request;
  list_blobs_request.bucket_name = bucket_name_;
  auto execution_result = JournalUtils::GetBlobFullPath(
      partition_name_, make_shared<string>(kJournalBlobNamePrefix),
      list_blobs_request.blob_name);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (start_from) {
    list_blobs_request.marker = start_from->blob_name;
    SCP_DEBUG_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                      "Listing journals from %s",
                      start_from->blob_name->c_str());
  } else {
    SCP_DEBUG_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                      "Listing all journal blobs");
  }

  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context(
      make_shared<ListBlobsRequest>(move(list_blobs_request)),
      bind(&JournalInputStream::OnListJournalsCallback, this,
           journal_stream_read_log_context, _1),
      journal_stream_read_log_context);

  return blob_storage_provider_client_->ListBlobs(list_blobs_context);
}

void JournalInputStream::OnListJournalsCallback(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    AsyncContext<ListBlobsRequest, ListBlobsResponse>&
        list_blobs_context) noexcept {
  if (!list_blobs_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kJournalInputStream, list_blobs_context,
                      list_blobs_context.result, "Failed to list journals");
    return FinishContext(list_blobs_context.result,
                         journal_stream_read_log_context);
  }

  SCP_INFO_CONTEXT(kJournalInputStream, list_blobs_context,
                   "Listed total %llu journal blobs",
                   list_blobs_context.response->blobs->size());

  auto stop_listing = false;
  for (const auto& journal_blob : *list_blobs_context.response->blobs) {
    JournalId journal_id = kInvalidJournalId;
    auto execution_result = JournalUtils::ExtractJournalId(
        partition_name_, journal_blob.blob_name, journal_id);
    if (!execution_result.Successful()) {
      return FinishContext(execution_result, journal_stream_read_log_context);
    }

    if (journal_id >
        journal_stream_read_log_context.request->max_journal_id_to_process) {
      stop_listing = true;
      break;
    }

    // All the journal ids with id greater than the last processed journal id
    // must be read.
    if (journal_id > last_processed_journal_id_) {
      journal_ids_.push_back(journal_id);
    }

    if (journal_ids_.size() >= journal_stream_read_log_context.request
                                   ->max_number_of_journals_to_process) {
      stop_listing = true;
      break;
    }
  }

  if (!stop_listing) {
    // If there are more journals remaining
    if (list_blobs_context.response->next_marker &&
        list_blobs_context.response->next_marker->blob_name->size() > 0) {
      auto execution_result =
          ListJournals(journal_stream_read_log_context,
                       list_blobs_context.response->next_marker);
      if (!execution_result.Successful()) {
        return FinishContext(execution_result, journal_stream_read_log_context);
      }
      return;
    }

    // Do one more listing to be sure that there are no more remaining.
    if (list_blobs_context.response->blobs->size() != 0) {
      auto next_marker =
          make_shared<Blob>(list_blobs_context.response->blobs->back());
      auto execution_result =
          ListJournals(journal_stream_read_log_context, next_marker);
      if (!execution_result.Successful()) {
        return FinishContext(execution_result, journal_stream_read_log_context);
      }
      return;
    }
  }

  if (enable_batch_read_journals_) {
    journal_ids_loaded_ = true;
  }

  if (!journal_ids_.empty()) {
    // Sort by id
    // TODO: Q) Is this necessary? This sort can be expensive.
    sort(journal_ids_.begin(), journal_ids_.end());

    // TODO: Q) Is this necessary for correctness? can't we pick the last one
    // in the list? Journals are already returned in lexicographical order.
    last_processed_journal_id_ = journal_ids_.back();

    SCP_INFO_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                     "Listing finished. Last journal id: %llu. Total number of "
                     "journals to be read is %llu",
                     last_processed_journal_id_, journal_ids_.size());

    auto execution_result =
        ReadJournalBlobs(journal_stream_read_log_context, journal_ids_);
    if (!execution_result.Successful()) {
      return FinishContext(execution_result, journal_stream_read_log_context);
    }
    return;
  }

  // No journals loaded.
  journals_loaded_ = true;

  // If the buffers is not empty, but no journal was read, it means checkpoint
  // buffer is present.
  if (journal_buffers_.empty()) {
    // No journals found and no checkpoint loaded. No logs to read, finish
    // context with End of Stream.
    journal_stream_read_log_context.result = FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN);
    journal_stream_read_log_context.Finish();
    return;
  }

  if (!journal_stream_read_log_context.request
           ->should_read_stream_when_only_checkpoint_exists) {
    // No journals found, checkpoint exists but need not be read.
    // No logs to read, finish context with End of Stream.
    journal_stream_read_log_context.result = FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN);
    journal_stream_read_log_context.Finish();
    return;
  }

  // Checkpoint buffer (also considered as a journal when it comes to
  // ProcessLoadedJournals) is loaded, so proceed with processing.
  auto execution_result =
      ProcessLoadedJournals(journal_stream_read_log_context);
  if (!execution_result.Successful()) {
    FinishContext(execution_result, journal_stream_read_log_context);
    return;
  }
}

ExecutionResult JournalInputStream::ReadJournalBlobs(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    vector<JournalId>& journal_ids) noexcept {
  if (!enable_batch_read_journals_) {
    if (journal_ids.empty()) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INPUT_STREAM_INVALID_LISTING);
    }
  }

  size_t total_journals_to_read = journal_ids.size();
  if (enable_batch_read_journals_) {
    size_t journal_ids_window_end_index_exclusive =
        journal_ids_window_start_index_ + journal_ids_window_length_;
    total_journals_to_read =
        std::min(number_of_journals_per_batch_,
                 journal_ids.size() - journal_ids_window_end_index_exclusive);
    if (total_journals_to_read == 0) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN);
    }
  }
  total_journals_to_read_ = total_journals_to_read;

  string journal_ids_string = absl::StrJoin(journal_ids, " ");

  SCP_DEBUG_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                    "All the journals to be read: %s",
                    journal_ids_string.c_str());

  size_t total_failed_read_journal = 0;
  size_t journal_buffer_current_size = journal_buffers_.size();
  journal_buffers_.resize(journal_buffer_current_size + total_journals_to_read);
  journal_ids_window_start_index_ =
      journal_ids_window_start_index_ + journal_ids_window_length_;
  journal_ids_window_length_ = total_journals_to_read;

  // Schedule journal reads in parallel, expect the last callback to continue
  // the async sequence.
  for (size_t i = 0; i < total_journals_to_read; i++) {
    size_t journal_ids_index_to_read = i;
    if (enable_batch_read_journals_) {
      journal_ids_index_to_read = journal_ids_window_start_index_ + i;
    }
    auto execution_result = ReadJournalBlob(
        journal_stream_read_log_context, journal_ids[journal_ids_index_to_read],
        journal_buffer_current_size + i);
    if (!execution_result.Successful()) {
      auto failed = false;
      // Only change if the current status was false.
      if (is_any_journal_read_failed_.compare_exchange_strong(failed, true)) {
        execution_result_of_failed_journal_read_ = execution_result;
      }

      total_failed_read_journal++;
    }
  }
  // Was this the last one?
  // Continuation is executed only by the last callback.
  if (total_failed_read_journal > 0 &&
      (total_journals_to_read_.fetch_sub(total_failed_read_journal) ==
       total_failed_read_journal)) {
    SCP_ERROR_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                      execution_result_of_failed_journal_read_.load(),
                      "Failed to read some of the journals.");
    return execution_result_of_failed_journal_read_;
  }

  return SuccessExecutionResult();
}

ExecutionResult JournalInputStream::ReadJournalBlob(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    JournalId journal_id, size_t buffer_index) noexcept {
  Blob journal_blob;
  journal_blob.bucket_name = bucket_name_;
  auto execution_result = JournalUtils::CreateJournalBlobName(
      partition_name_, journal_id, journal_blob.blob_name);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  GetBlobRequest get_blob_request;
  get_blob_request.bucket_name = journal_blob.bucket_name;
  get_blob_request.blob_name = journal_blob.blob_name;

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      make_shared<GetBlobRequest>(move(get_blob_request)),
      bind(&JournalInputStream::OnReadJournalBlobCallback, this,
           journal_stream_read_log_context, _1, buffer_index),
      journal_stream_read_log_context);

  return blob_storage_provider_client_->GetBlob(get_blob_context);
}

void JournalInputStream::OnReadJournalBlobCallback(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context,
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context,
    size_t buffer_index) noexcept {
  if (!get_blob_context.result.Successful()) {
    auto failed = false;
    // Only change if the current status was false.
    if (is_any_journal_read_failed_.compare_exchange_strong(failed, true)) {
      execution_result_of_failed_journal_read_ =
          FailureExecutionResult(get_blob_context.result.status_code);
    }
  } else {
    BytesBuffer& bytes_buffer = journal_buffers_[buffer_index];
    bytes_buffer.bytes.swap(get_blob_context.response->buffer->bytes);
    bytes_buffer.length = get_blob_context.response->buffer->length;
    bytes_buffer.capacity = get_blob_context.response->buffer->capacity;
  }

  // Was it the last callback?
  // Continuation is executed only by the last callback.
  if (total_journals_to_read_.fetch_sub(1) != 1) {
    return;
  }

  // The last callback is executing this.
  if (is_any_journal_read_failed_) {
    SCP_ERROR_CONTEXT(kJournalInputStream, journal_stream_read_log_context,
                      execution_result_of_failed_journal_read_.load(),
                      "Failed to read some of the journals");
    return FinishContext(execution_result_of_failed_journal_read_,
                         journal_stream_read_log_context);
  }

  // All of the journals are loaded into the memory.
  journals_loaded_ = true;

  auto execution_result =
      ProcessLoadedJournals(journal_stream_read_log_context);
  if (!execution_result.Successful()) {
    FinishContext(execution_result, journal_stream_read_log_context);
    return;
  }
}

ExecutionResult JournalInputStream::ProcessLoadedJournals(
    AsyncContext<JournalStreamReadLogRequest, JournalStreamReadLogResponse>&
        journal_stream_read_log_context) noexcept {
  auto logs = make_shared<vector<JournalStreamReadLogObject>>();
  auto execution_result = ReadJournalLogBatch(logs);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  FinishContextWithResponse(journal_stream_read_log_context, move(logs));
  return SuccessExecutionResult();
}

JournalId JournalInputStream::GetCurrentBufferJournalId() {
  // When last checkpoint file is not available, the current_buffer_index_ =
  // 0 is pointing to the first element in the journal_ids_ vector
  //
  // When the last checkpoint file is available:
  // - If the journal_ids window is pointing at the beginning of the list
  //   - current_buffer_index_ = 0 is pointing to the checkpoint file, and the
  //     current_buffer_index_ = 1 is pointing journal_ids_[0]
  // - Otherwise, current_buffer_index_ is pointing to the journal_ids at
  //   journal_ids_window_start_index_ + current_buffer_index_
  bool is_checkpoint_buffer_at_begin =
      last_checkpoint_id_ != kInvalidCheckpointId;

  if (enable_batch_read_journals_) {
    if (!is_checkpoint_buffer_at_begin) {
      return journal_ids_[journal_ids_window_start_index_ +
                          current_buffer_index_];
    }
    if (journal_ids_window_start_index_ > 0) {
      return journal_ids_[journal_ids_window_start_index_ +
                          current_buffer_index_];
    }
    if (current_buffer_index_ == 0) {
      return last_checkpoint_id_;
    }
    return journal_ids_[current_buffer_index_ - 1];
  }

  // The buffer at index '0' is (optionally) occupied by checkpoint.
  if (is_checkpoint_buffer_at_begin) {
    if (current_buffer_index_ == 0) {
      return last_checkpoint_id_;
    }
    return journal_ids_[current_buffer_index_ - 1];
  }
  return journal_ids_[current_buffer_index_];
}

ExecutionResult JournalInputStream::ProcessNextJournalLog(
    Timestamp& timestamp, JournalLogStatus& journal_log_status,
    Uuid& component_id, Uuid& log_id, JournalLog& journal_log,
    JournalId& journal_id) noexcept {
  if (current_buffer_index_ >= journal_buffers_.size()) {
    return FailureExecutionResult(
        errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN);
  }

  if (current_buffer_offset_ >=
      journal_buffers_[current_buffer_index_].length) {
    current_buffer_offset_ = 0;
    current_buffer_index_++;
    return ProcessNextJournalLog(timestamp, journal_log_status, component_id,
                                 log_id, journal_log, journal_id);
  }

  // current_buffer_offset_ < journal_buffers_[current_buffer_index_].length)
  size_t bytes_deserialized = 0;
  auto execution_result = JournalSerialization::DeserializeLogHeader(
      journal_buffers_[current_buffer_index_], current_buffer_offset_,
      timestamp, journal_log_status, component_id, log_id, bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  current_buffer_offset_ += bytes_deserialized;

  bytes_deserialized = 0;
  execution_result = JournalSerialization::DeserializeJournalLog(
      journal_buffers_[current_buffer_index_], current_buffer_offset_,
      journal_log, bytes_deserialized);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  journal_id = GetCurrentBufferJournalId();
  current_buffer_offset_ += bytes_deserialized;
  return SuccessExecutionResult();
}

ExecutionResult JournalInputStream::ReadJournalLogBatch(
    std::shared_ptr<std::vector<JournalStreamReadLogObject>>&
        journal_batch) noexcept {
  ExecutionResult last_execution_result = SuccessExecutionResult();
  for (size_t i = 0; i < number_of_journal_logs_to_return_; ++i) {
    // Extract log and send a response
    JournalStreamReadLogObject journal_stream_read_log_object;
    journal_stream_read_log_object.journal_log = make_shared<JournalLog>();

    auto execution_result =
        ProcessNextJournalLog(journal_stream_read_log_object.timestamp,
                              journal_stream_read_log_object.log_status,
                              journal_stream_read_log_object.component_id,
                              journal_stream_read_log_object.log_id,
                              *journal_stream_read_log_object.journal_log,
                              journal_stream_read_log_object.journal_id);
    if (!execution_result.Successful()) {
      if (execution_result ==
          FailureExecutionResult(
              errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN)) {
        last_execution_result = execution_result;
        break;
      }
      return execution_result;
    }

    journal_batch->push_back(move(journal_stream_read_log_object));
  }

  if (enable_batch_read_journals_) {
    // When current_buffer_offset_ is already out of bound but not yet updated,
    // update it to the next valid location
    if (current_buffer_index_ < journal_buffers_.size() &&
        current_buffer_offset_ >=
            journal_buffers_[current_buffer_index_].length) {
      current_buffer_offset_ = 0;
      current_buffer_index_++;
    }
    if (current_buffer_index_ >= journal_buffers_.size()) {
      journal_ids_window_start_index_ += journal_ids_window_length_;
      journal_ids_window_length_ = 0;
      journal_buffers_.clear();
      current_buffer_index_ = 0;
      current_buffer_offset_ = 0;
    } else if (current_buffer_index_ > 0) {
      // Deallocate bytes that will no longer be used.
      for (size_t i = 0; i < current_buffer_index_; i++) {
        BytesBuffer& bytes_buffer = journal_buffers_[i];
        if (bytes_buffer.length != 0) {
          bytes_buffer.Reset();
        }
      }
    }
  }

  // Return partial stream without any error if possible
  if (last_execution_result ==
          FailureExecutionResult(
              errors::SC_JOURNAL_SERVICE_INPUT_STREAM_NO_MORE_LOGS_TO_RETURN) &&
      journal_batch->size() == 0) {
    return last_execution_result;
  }

  return SuccessExecutionResult();
}

JournalId JournalInputStream::GetLastProcessedJournalId() noexcept {
  return last_processed_journal_id_;
};
}  // namespace google::scp::core
