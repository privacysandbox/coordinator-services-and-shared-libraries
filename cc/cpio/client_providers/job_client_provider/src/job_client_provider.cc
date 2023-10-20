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

#include "job_client_provider.h"

#include <memory>
#include <string>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/type_def.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "google/protobuf/util/time_util.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"

#include "error_codes.h"
#include "job_client_utils.h"

using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest;
using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse;
using google::cmrt::sdk::job_service::v1::GetJobByIdRequest;
using google::cmrt::sdk::job_service::v1::GetJobByIdResponse;
using google::cmrt::sdk::job_service::v1::GetNextJobRequest;
using google::cmrt::sdk::job_service::v1::GetNextJobResponse;
using google::cmrt::sdk::job_service::v1::Job;
using google::cmrt::sdk::job_service::v1::JobStatus;
using google::cmrt::sdk::job_service::v1::PutJobRequest;
using google::cmrt::sdk::job_service::v1::PutJobResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::ItemAttribute;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse;
using google::cmrt::sdk::queue_service::v1::DeleteMessageRequest;
using google::cmrt::sdk::queue_service::v1::DeleteMessageResponse;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageRequest;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageResponse;
using google::cmrt::sdk::queue_service::v1::GetTopMessageRequest;
using google::cmrt::sdk::queue_service::v1::GetTopMessageResponse;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutRequest;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutResponse;
using google::protobuf::Duration;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_DUPLICATE_JOB_ENTRY_CREATION;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_DURATION;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO;
using google::scp::core::errors::
    SC_JOB_CLIENT_PROVIDER_JOB_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_SERIALIZATION_FAILED;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_UPDATION_CONFLICT;
using google::scp::core::errors::
    SC_NO_SQL_DATABASE_PROVIDER_CONDITIONAL_CHECKED_FAILED;
using google::scp::core::errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

namespace {
constexpr char kJobClientProvider[] = "JobClientProvider";
constexpr char kProcessingStartedTimeColumnName[] = "ProcessingStartedTime";
constexpr int kDefaultRetryCount = 0;
constexpr int kMaximumVisibilityTimeoutInSeconds = 600;
const google::protobuf::Timestamp kDefaultTimestampValue =
    TimeUtil::SecondsToTimestamp(0);

}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult JobClientProvider::Init() noexcept {
  if (!job_client_options_) {
    auto execution_result = FailureExecutionResult(
        SC_JOB_CLIENT_PROVIDER_JOB_CLIENT_OPTIONS_REQUIRED);
    SCP_ERROR(kJobClientProvider, kZeroUuid, execution_result,
              "Invalid job client options.");
    return execution_result;
  }

  job_table_name_ = move(job_client_options_->job_table_name);

  return SuccessExecutionResult();
}

ExecutionResult JobClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult JobClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult JobClientProvider::PutJob(
    AsyncContext<PutJobRequest, PutJobResponse>& put_job_context) noexcept {
  const string& job_id = put_job_context.request->job_id();
  if (job_id.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID);
    SCP_ERROR_CONTEXT(kJobClientProvider, put_job_context, execution_result,
                      "Failed to put job due to missing job id.");
    put_job_context.result = execution_result;
    put_job_context.Finish();
    return execution_result;
  }

  auto enqueue_message_request = make_shared<EnqueueMessageRequest>();
  enqueue_message_request->set_message_body(job_id);
  AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>
      enqueue_message_context(move(enqueue_message_request),
                              bind(&JobClientProvider::OnEnqueueMessageCallback,
                                   this, put_job_context, _1),
                              put_job_context);

  return queue_client_provider_->EnqueueMessage(enqueue_message_context);
}

void JobClientProvider::OnEnqueueMessageCallback(
    AsyncContext<PutJobRequest, PutJobResponse>& put_job_context,
    AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
        enqueue_message_context) noexcept {
  const string& job_id = enqueue_message_context.request->message_body();
  if (!enqueue_message_context.result.Successful()) {
    auto execution_result = enqueue_message_context.result;
    SCP_ERROR_CONTEXT(
        kJobClientProvider, put_job_context, execution_result,
        "Failed to put job due to job message creation failed. Job id: %s",
        job_id.c_str());
    put_job_context.result = execution_result;
    put_job_context.Finish();
    return;
  }

  const string& server_job_id = enqueue_message_context.response->message_id();
  const auto& job_body = put_job_context.request->job_body();
  const auto job_body_as_string_or =
      JobClientUtils::ConvertAnyToBase64String(job_body);
  if (!job_body_as_string_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, put_job_context, job_body_as_string_or.result(),
        "Cannot serialize the job body. Job id: %s", job_id.c_str());
    put_job_context.result = job_body_as_string_or.result();
    put_job_context.Finish();
    return;
  }

  auto current_time = TimeUtil::GetCurrentTime();
  auto job = make_shared<Job>(JobClientUtils::CreateJob(
      job_id, server_job_id, job_body, JobStatus::JOB_STATUS_CREATED,
      current_time, current_time, kDefaultTimestampValue, kDefaultRetryCount));

  auto upsert_job_request = JobClientUtils::CreatePutJobRequest(
      job_table_name_, *job, *job_body_as_string_or);

  // Add processing_started_time field with default value for job creation.
  *upsert_job_request->add_new_attributes() =
      JobClientUtils::MakeStringAttribute(
          kProcessingStartedTimeColumnName,
          TimeUtil::ToString(job->processing_started_time()));

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context(
          move(upsert_job_request),
          bind(&JobClientProvider::OnUpsertNewJobItemCallback, this,
               put_job_context, move(job), _1),
          put_job_context);
  auto execution_result = nosql_database_client_provider_->UpsertDatabaseItem(
      upsert_database_item_context);

  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kJobClientProvider, put_job_context, execution_result,
                      "Cannot upsert job into NoSQL database. Job id: %s",
                      job_id.c_str());
    put_job_context.result = execution_result;
    put_job_context.Finish();
    return;
  }
}

void JobClientProvider::OnUpsertNewJobItemCallback(
    AsyncContext<PutJobRequest, PutJobResponse>& put_job_context,
    shared_ptr<Job> job,
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context) noexcept {
  auto exeuction_result = upsert_database_item_context.result;
  if (!exeuction_result.Successful()) {
    auto converted_result =
        ConvertDatabaseErrorForPutJob(exeuction_result.status_code);
    const string& job_id = upsert_database_item_context.request->key()
                               .partition_key()
                               .value_string();
    SCP_ERROR_CONTEXT(kJobClientProvider, put_job_context, converted_result,
                      "Failed to put job due to upsert job to NoSQL database "
                      "failed. Job id: %s",
                      job_id.c_str());
    put_job_context.result = converted_result;
    put_job_context.Finish();
    return;
  }

  put_job_context.response = make_shared<PutJobResponse>();
  *put_job_context.response->mutable_job() = move(*job);
  put_job_context.result = SuccessExecutionResult();
  put_job_context.Finish();
}

ExecutionResult JobClientProvider::GetNextJob(
    AsyncContext<GetNextJobRequest, GetNextJobResponse>&
        get_next_job_context) noexcept {
  AsyncContext<GetTopMessageRequest, GetTopMessageResponse>
      get_top_message_context(move(make_shared<GetTopMessageRequest>()),
                              bind(&JobClientProvider::OnGetTopMessageCallback,
                                   this, get_next_job_context, _1),
                              get_next_job_context);

  return queue_client_provider_->GetTopMessage(get_top_message_context);
}

void JobClientProvider::OnGetTopMessageCallback(
    AsyncContext<GetNextJobRequest, GetNextJobResponse>& get_next_job_context,
    AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
        get_top_message_context) noexcept {
  if (!get_top_message_context.result.Successful()) {
    auto execution_result = get_top_message_context.result;
    SCP_ERROR_CONTEXT(
        kJobClientProvider, get_next_job_context, execution_result,
        "Failed to get next job due to get job message from queue failed.");
    get_next_job_context.result = execution_result;
    get_next_job_context.Finish();
    return;
  }

  const string& server_job_id = get_top_message_context.response->message_id();
  if (server_job_id.empty()) {
    get_next_job_context.response = make_shared<GetNextJobResponse>();
    SCP_INFO_CONTEXT(kJobClientProvider, get_top_message_context,
                     "No jobs received from the job queue.");
    get_next_job_context.result = get_top_message_context.result;
    get_next_job_context.Finish();
    return;
  }

  const string& job_id = get_top_message_context.response->message_body();
  shared_ptr<string> receipt_info(
      get_top_message_context.response->release_receipt_info());

  auto get_database_item_request = JobClientUtils::CreateGetNextJobRequest(
      job_table_name_, job_id, server_job_id);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          move(get_database_item_request),
          bind(&JobClientProvider::OnGetNextJobItemCallback, this,
               get_next_job_context, move(receipt_info), _1),
          get_next_job_context);
  auto execution_result = nosql_database_client_provider_->GetDatabaseItem(
      get_database_item_context);

  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, get_next_job_context, execution_result,
        "Cannot get job from NoSQL database. Job id: %s", job_id.c_str());
    get_next_job_context.result = execution_result;
    get_next_job_context.Finish();
    return;
  }
}

void JobClientProvider::OnGetNextJobItemCallback(
    AsyncContext<GetNextJobRequest, GetNextJobResponse>& get_next_job_context,
    shared_ptr<string> receipt_info,
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  const string& job_id =
      get_database_item_context.request->key().partition_key().value_string();
  if (get_database_item_context.request->required_attributes().size() == 0) {
    get_next_job_context.result =
        FailureExecutionResult(SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);
    get_next_job_context.Finish();
    return;
    SCP_ERROR_CONTEXT(kJobClientProvider, get_next_job_context,
                      get_next_job_context.result,
                      "Missing server job id in the job entry in the NoSQL "
                      "database. Job id: %s",
                      job_id.c_str());
  }
  const string& server_job_id =
      get_database_item_context.request->required_attributes(0).value_string();
  if (!get_database_item_context.result.Successful()) {
    auto execution_result = get_database_item_context.result;
    if (execution_result.status_code ==
        SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND) {
      SCP_ERROR_CONTEXT(
          kJobClientProvider, get_next_job_context, execution_result,
          "Failed to get next job due to the job entry is missing in the "
          "NoSQL database, or the server job id in the job message in the "
          "queue does not match the one in the job entry in the NoSQL "
          "database. Job id: %s, server job id: %s",
          job_id.c_str(), server_job_id.c_str());
    } else {
      SCP_ERROR_CONTEXT(kJobClientProvider, get_next_job_context,
                        execution_result,
                        "Failed to get next job due to get job from NoSQL "
                        "database failed. Job id: %s",
                        job_id.c_str());
    }
    get_next_job_context.result = execution_result;
    get_next_job_context.Finish();
    return;
  }

  const auto& item = get_database_item_context.response->item();
  auto job_or = JobClientUtils::ConvertDatabaseItemToJob(item);
  if (!job_or.Successful()) {
    SCP_ERROR_CONTEXT(kJobClientProvider, get_next_job_context, job_or.result(),
                      "Cannot convert database item to job. Job id: %s",
                      job_id.c_str());
    get_next_job_context.result = job_or.result();
    get_next_job_context.Finish();
    return;
  }

  get_next_job_context.response = make_shared<GetNextJobResponse>();
  *get_next_job_context.response->mutable_job() = move(*job_or);
  *get_next_job_context.response->mutable_receipt_info() = move(*receipt_info);
  get_next_job_context.result = SuccessExecutionResult();
  get_next_job_context.Finish();
}

ExecutionResult JobClientProvider::GetJobById(
    AsyncContext<GetJobByIdRequest, GetJobByIdResponse>&
        get_job_by_id_context) noexcept {
  const string& job_id = get_job_by_id_context.request->job_id();
  if (job_id.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID);
    SCP_ERROR_CONTEXT(kJobClientProvider, get_job_by_id_context,
                      execution_result,
                      "Failed to get job by id due to missing job id.");
    get_job_by_id_context.result = execution_result;
    get_job_by_id_context.Finish();
    return execution_result;
  }
  auto get_database_item_request =
      JobClientUtils::CreateGetJobByJobIdRequest(job_table_name_, job_id);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          move(get_database_item_request),
          bind(&JobClientProvider::OnGetJobItemByJobIdCallback, this,
               get_job_by_id_context, _1),
          get_job_by_id_context);

  return nosql_database_client_provider_->GetDatabaseItem(
      get_database_item_context);
}

void JobClientProvider::OnGetJobItemByJobIdCallback(
    AsyncContext<GetJobByIdRequest, GetJobByIdResponse>& get_job_by_id_context,
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  const string& job_id = get_job_by_id_context.request->job_id();
  if (!get_database_item_context.result.Successful()) {
    auto execution_result = get_database_item_context.result;
    if (execution_result.status_code ==
        SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND) {
      SCP_ERROR_CONTEXT(kJobClientProvider, get_job_by_id_context,
                        execution_result,
                        "Failed to get job by job id due to the entry for the "
                        "job id %s is missing in the NoSQL database.",
                        job_id.c_str());
    } else {
      SCP_ERROR_CONTEXT(kJobClientProvider, get_job_by_id_context,
                        execution_result,
                        "Failed to get job by job id due to get job from NoSQL "
                        "database failed. Job id: %s",
                        job_id.c_str());
    }
    get_job_by_id_context.result = execution_result;
    get_job_by_id_context.Finish();
    return;
  }

  const auto& item = get_database_item_context.response->item();
  auto job_or = JobClientUtils::ConvertDatabaseItemToJob(item);
  if (!job_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, get_job_by_id_context, job_or.result(),
        "Cannot convert database item to job. Job id: %s", job_id.c_str());
    get_job_by_id_context.result = job_or.result();
    get_job_by_id_context.Finish();
    return;
  }

  get_job_by_id_context.response = make_shared<GetJobByIdResponse>();
  *get_job_by_id_context.response->mutable_job() = move(*job_or);
  get_job_by_id_context.result = SuccessExecutionResult();
  get_job_by_id_context.Finish();
}

ExecutionResult JobClientProvider::UpdateJobBody(
    AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse>&
        update_job_body_context) noexcept {
  const string& job_id = update_job_body_context.request->job_id();
  if (job_id.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID);
    SCP_ERROR_CONTEXT(kJobClientProvider, update_job_body_context,
                      execution_result,
                      "Failed to update job body due to missing job id.");
    update_job_body_context.result = execution_result;
    update_job_body_context.Finish();
    return execution_result;
  }

  auto get_database_item_request =
      JobClientUtils::CreateGetJobByJobIdRequest(job_table_name_, job_id);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          move(get_database_item_request),
          bind(&JobClientProvider::OnGetJobItemForUpdateJobBodyCallback, this,
               update_job_body_context, _1),
          update_job_body_context);

  return nosql_database_client_provider_->GetDatabaseItem(
      get_database_item_context);
}

void JobClientProvider::OnGetJobItemForUpdateJobBodyCallback(
    AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse>&
        update_job_body_context,
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  const string& job_id = update_job_body_context.request->job_id();
  if (!get_database_item_context.result.Successful()) {
    auto execution_result = get_database_item_context.result;
    SCP_ERROR_CONTEXT(kJobClientProvider, update_job_body_context,
                      execution_result,
                      "Failed to update job body due to get job from NoSQL "
                      "database failed. Job id: %s",
                      job_id.c_str());
    update_job_body_context.result = execution_result;
    update_job_body_context.Finish();
    return;
  }

  const auto& item = get_database_item_context.response->item();
  auto job_or = JobClientUtils::ConvertDatabaseItemToJob(item);
  if (!job_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_body_context, job_or.result(),
        "Cannot convert database item to job. Job id: %s", job_id.c_str());
    update_job_body_context.result = job_or.result();
    update_job_body_context.Finish();
    return;
  }

  if (job_or->updated_time() >
      update_job_body_context.request->most_recent_updated_time()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_UPDATION_CONFLICT);
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_body_context, execution_result,
        "Failed to update job body due to job is already updated by "
        "another request. Job id: %s",
        job_id.c_str());
    update_job_body_context.result = execution_result;
    update_job_body_context.Finish();
    return;
  }

  Job job_for_update;
  job_for_update.set_allocated_job_id(
      update_job_body_context.request->release_job_id());
  auto update_time =
      make_shared<google::protobuf::Timestamp>(TimeUtil::GetCurrentTime());
  *job_for_update.mutable_updated_time() = *update_time;

  const auto& job_body = update_job_body_context.request->job_body();
  const auto job_body_as_string_or =
      JobClientUtils::ConvertAnyToBase64String(job_body);
  if (!job_body_as_string_or.Successful()) {
    SCP_ERROR_CONTEXT(kJobClientProvider, update_job_body_context,
                      job_body_as_string_or.result(),
                      "Cannot serialize the job body. Job id: %s",
                      job_id.c_str());
    update_job_body_context.result = job_body_as_string_or.result();
    update_job_body_context.Finish();
    return;
  }

  auto upsert_job_request = JobClientUtils::CreateUpsertJobRequest(
      job_table_name_, job_for_update, *job_body_as_string_or);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context(
          move(upsert_job_request),
          bind(&JobClientProvider::OnUpsertUpdatedJobBodyJobItemCallback, this,
               update_job_body_context, move(update_time), _1),
          update_job_body_context);

  auto execution_result = nosql_database_client_provider_->UpsertDatabaseItem(
      upsert_database_item_context);

  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_body_context, execution_result,
        "Cannot upsert job into NoSQL database. Job id: %s", job_id.c_str());
    update_job_body_context.result = execution_result;
    update_job_body_context.Finish();
    return;
  }
}

void JobClientProvider::OnUpsertUpdatedJobBodyJobItemCallback(
    AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse>&
        update_job_body_context,
    shared_ptr<google::protobuf::Timestamp> update_time,
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context) noexcept {
  if (!upsert_database_item_context.result.Successful()) {
    auto execution_result = upsert_database_item_context.result;
    SCP_ERROR_CONTEXT(kJobClientProvider, update_job_body_context,
                      execution_result,
                      "Failed to update job body due to upsert updated job to "
                      "NoSQL database failed. Job id: %s",
                      upsert_database_item_context.request->key()
                          .partition_key()
                          .value_string()
                          .c_str());
    update_job_body_context.result = execution_result;
    update_job_body_context.Finish();
    return;
  }

  update_job_body_context.response = make_shared<UpdateJobBodyResponse>();
  *update_job_body_context.response->mutable_updated_time() = *update_time;
  update_job_body_context.result = SuccessExecutionResult();
  update_job_body_context.Finish();
}

ExecutionResult JobClientProvider::UpdateJobStatus(
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context) noexcept {
  const string& job_id = update_job_status_context.request->job_id();
  if (job_id.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID);
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_status_context, execution_result,
        "Failed to update status due to missing job id in the request.");
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return execution_result;
  }

  const string& receipt_info =
      update_job_status_context.request->receipt_info();
  const auto& job_status = update_job_status_context.request->job_status();
  if (receipt_info.empty() && (job_status == JobStatus::JOB_STATUS_SUCCESS ||
                               job_status == JobStatus::JOB_STATUS_FAILURE)) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO);
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_status_context, execution_result,
        "Failed to update status due to missing receipt info in the "
        "request. Job id: %s",
        job_id.c_str());
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return execution_result;
  }

  auto get_database_item_request =
      JobClientUtils::CreateGetJobByJobIdRequest(job_table_name_, job_id);

  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          move(get_database_item_request),
          bind(&JobClientProvider::OnGetJobItemForUpdateJobStatusCallback, this,
               update_job_status_context, _1),
          update_job_status_context);

  return nosql_database_client_provider_->GetDatabaseItem(
      get_database_item_context);
}

void JobClientProvider::OnGetJobItemForUpdateJobStatusCallback(
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context,
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  const string& job_id = update_job_status_context.request->job_id();
  if (!get_database_item_context.result.Successful()) {
    auto execution_result = get_database_item_context.result;
    SCP_ERROR_CONTEXT(kJobClientProvider, update_job_status_context,
                      execution_result,
                      "Failed to update job status due to get job from NoSQL "
                      "database failed. Job id: %s",
                      job_id.c_str());
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return;
  }

  const auto& item = get_database_item_context.response->item();
  auto job_or = JobClientUtils::ConvertDatabaseItemToJob(item);
  if (!job_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_status_context, job_or.result(),
        "Cannot convert database item to job. Job id: %s", job_id.c_str());
    update_job_status_context.result = job_or.result();
    update_job_status_context.Finish();
    return;
  }

  if (job_or->updated_time() >
      update_job_status_context.request->most_recent_updated_time()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_UPDATION_CONFLICT);
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_status_context, execution_result,
        "Failed to update job status due to job is already updated "
        "by another request. Job id: %s",
        job_id.c_str());
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return;
  }

  const JobStatus& current_job_status = job_or->job_status();
  const JobStatus& job_status_in_request =
      update_job_status_context.request->job_status();
  auto execution_result = JobClientUtils::ValidateJobStatus(
      current_job_status, job_status_in_request);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_status_context, execution_result,
        "Failed to update status due to invalid job status. Job id: "
        "%s, Current Job status: %s, Job status in request: %s",
        job_id.c_str(), current_job_status, job_status_in_request);
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return;
  }

  int retry_count = job_or->retry_count();
  switch (job_status_in_request) {
    // TODO: Add new failure status for retry mechanism.
    case JobStatus::JOB_STATUS_PROCESSING:
      retry_count++;
    case JobStatus::JOB_STATUS_FAILURE:
    case JobStatus::JOB_STATUS_SUCCESS: {
      UpsertUpdatedJobStatusJobItem(update_job_status_context, retry_count);
      return;
    }
    case JobStatus::JOB_STATUS_UNKNOWN:
    case JobStatus::JOB_STATUS_CREATED:
    default: {
      auto execution_result =
          FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS);
      SCP_ERROR_CONTEXT(
          kJobClientProvider, update_job_status_context, execution_result,
          "Failed to update status due to invalid job status in the "
          "request. Job id: %s, Job status: %s",
          job_id.c_str(), job_status_in_request);
      update_job_status_context.result = execution_result;
      update_job_status_context.Finish();
    }
  }
}

void JobClientProvider::UpsertUpdatedJobStatusJobItem(
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context,
    const int retry_count) noexcept {
  const string& job_id = update_job_status_context.request->job_id();
  auto update_time =
      make_shared<google::protobuf::Timestamp>(TimeUtil::GetCurrentTime());

  Job job_for_update;
  job_for_update.set_allocated_job_id(
      update_job_status_context.request->release_job_id());
  *job_for_update.mutable_updated_time() = *update_time;
  auto job_status_in_request = update_job_status_context.request->job_status();
  job_for_update.set_job_status(job_status_in_request);
  job_for_update.set_retry_count(retry_count);
  if (job_status_in_request == JobStatus::JOB_STATUS_PROCESSING) {
    *job_for_update.mutable_processing_started_time() = *update_time;
  }

  auto upsert_job_request = JobClientUtils::CreateUpsertJobRequest(
      job_table_name_, job_for_update, "" /* job body */);

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      upsert_database_item_context(
          move(upsert_job_request),
          bind(&JobClientProvider::OnUpsertUpdatedJobStatusJobItemCallback,
               this, update_job_status_context, move(update_time), retry_count,
               _1),
          update_job_status_context);

  auto execution_result = nosql_database_client_provider_->UpsertDatabaseItem(
      upsert_database_item_context);

  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_status_context, execution_result,
        "Cannot upsert job into NoSQL database. Job id: %s", job_id.c_str());
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return;
  }
}

void JobClientProvider::OnUpsertUpdatedJobStatusJobItemCallback(
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context,
    shared_ptr<google::protobuf::Timestamp> update_time, const int retry_count,
    AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>&
        upsert_database_item_context) noexcept {
  if (!upsert_database_item_context.result.Successful()) {
    auto execution_result = upsert_database_item_context.result;
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_status_context, execution_result,
        "Failed to update job status due to upsert updated job to "
        "NoSQL database failed. Job id: %s",
        update_job_status_context.request->job_id().c_str());
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return;
  }

  auto job_status_in_request = update_job_status_context.request->job_status();
  switch (job_status_in_request) {
    case JobStatus::JOB_STATUS_FAILURE:
    case JobStatus::JOB_STATUS_SUCCESS:
      DeleteJobMessageForUpdatingJobStatus(update_job_status_context,
                                           update_time, retry_count);
      return;
    case JobStatus::JOB_STATUS_PROCESSING:
    default:
      update_job_status_context.response =
          make_shared<UpdateJobStatusResponse>();
      update_job_status_context.response->set_job_status(job_status_in_request);
      *update_job_status_context.response->mutable_updated_time() =
          *update_time;
      update_job_status_context.response->set_retry_count(retry_count);
      update_job_status_context.result = SuccessExecutionResult();
      update_job_status_context.Finish();
  }
}

void JobClientProvider::DeleteJobMessageForUpdatingJobStatus(
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context,
    shared_ptr<google::protobuf::Timestamp> update_time,
    const int retry_count) noexcept {
  const string& job_id = update_job_status_context.request->job_id();

  auto delete_message_request = make_shared<DeleteMessageRequest>();
  delete_message_request->set_allocated_receipt_info(
      update_job_status_context.request->release_receipt_info());
  AsyncContext<DeleteMessageRequest, DeleteMessageResponse>
      delete_message_context(
          move(delete_message_request),
          bind(&JobClientProvider::
                   OnDeleteJobMessageForUpdatingJobStatusCallback,
               this, update_job_status_context, move(update_time), retry_count,
               _1),
          update_job_status_context);

  auto execution_result =
      queue_client_provider_->DeleteMessage(delete_message_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_status_context, execution_result,
        "Cannot delete message from queue. Job id: %s", job_id.c_str());
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return;
  }
}

void JobClientProvider::OnDeleteJobMessageForUpdatingJobStatusCallback(
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context,
    shared_ptr<google::protobuf::Timestamp> update_time, const int retry_count,
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_messasge_context) noexcept {
  const string& job_id = update_job_status_context.request->job_id();
  if (!delete_messasge_context.result.Successful()) {
    auto execution_result = delete_messasge_context.result;
    SCP_ERROR_CONTEXT(kJobClientProvider, update_job_status_context,
                      execution_result,
                      "Failed to update job status due to job message deletion "
                      "failed. Job id; %s",
                      job_id.c_str());
    update_job_status_context.result = execution_result;
    update_job_status_context.Finish();
    return;
  }

  update_job_status_context.response = make_shared<UpdateJobStatusResponse>();
  update_job_status_context.response->set_job_status(
      update_job_status_context.request->job_status());
  *update_job_status_context.response->mutable_updated_time() = *update_time;
  update_job_status_context.response->set_retry_count(retry_count);
  update_job_status_context.result = SuccessExecutionResult();
  update_job_status_context.Finish();
}

ExecutionResult JobClientProvider::UpdateJobVisibilityTimeout(
    AsyncContext<UpdateJobVisibilityTimeoutRequest,
                 UpdateJobVisibilityTimeoutResponse>&
        update_job_visibility_timeout_context) noexcept {
  const string& job_id =
      update_job_visibility_timeout_context.request->job_id();
  if (job_id.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID);
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_visibility_timeout_context,
        execution_result,
        "Failed to update visibility timeout due to missing job id "
        "in the request.");
    update_job_visibility_timeout_context.result = execution_result;
    update_job_visibility_timeout_context.Finish();
    return execution_result;
  }

  const auto& duration =
      update_job_visibility_timeout_context.request->duration_to_update();
  if (duration.seconds() < 0 ||
      duration.seconds() > kMaximumVisibilityTimeoutInSeconds) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_DURATION);
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_visibility_timeout_context,
        execution_result,
        "Failed to update visibility timeout due to invalid duration "
        "in the request. Job id: %s, duration: %d",
        job_id.c_str(), duration.seconds());
    update_job_visibility_timeout_context.result = execution_result;
    update_job_visibility_timeout_context.Finish();
    return execution_result;
  }

  const string& receipt_info =
      update_job_visibility_timeout_context.request->receipt_info();
  if (receipt_info.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO);
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_visibility_timeout_context,
        execution_result,
        "Failed to update visibility timeout due to missing receipt "
        "info in the request. Job id: %s",
        job_id.c_str());
    update_job_visibility_timeout_context.result = execution_result;
    update_job_visibility_timeout_context.Finish();
    return execution_result;
  }

  auto update_message_visibility_timeout_request =
      make_shared<UpdateMessageVisibilityTimeoutRequest>();
  *update_message_visibility_timeout_request
       ->mutable_message_visibility_timeout() =
      update_job_visibility_timeout_context.request->duration_to_update();
  update_message_visibility_timeout_request->set_allocated_receipt_info(
      update_job_visibility_timeout_context.request->release_receipt_info());

  AsyncContext<UpdateMessageVisibilityTimeoutRequest,
               UpdateMessageVisibilityTimeoutResponse>
      update_message_visibility_timeout_context(
          move(update_message_visibility_timeout_request),
          bind(&JobClientProvider::OnUpdateMessageVisibilityTimeoutCallback,
               this, update_job_visibility_timeout_context, _1),
          update_job_visibility_timeout_context);

  return queue_client_provider_->UpdateMessageVisibilityTimeout(
      update_message_visibility_timeout_context);
}

void JobClientProvider::OnUpdateMessageVisibilityTimeoutCallback(
    AsyncContext<UpdateJobVisibilityTimeoutRequest,
                 UpdateJobVisibilityTimeoutResponse>&
        update_job_visibility_timeout_context,
    AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                 UpdateMessageVisibilityTimeoutResponse>&
        update_message_visibility_timeout_context) noexcept {
  string* job_id =
      update_job_visibility_timeout_context.request->release_job_id();
  if (!update_message_visibility_timeout_context.result.Successful()) {
    auto execution_result = update_message_visibility_timeout_context.result;
    SCP_ERROR_CONTEXT(
        kJobClientProvider, update_job_visibility_timeout_context,
        execution_result,
        "Failed to update job visibility timeout due to update job "
        "message visibility tiemout failed. Job id; %s",
        job_id->c_str());
    update_job_visibility_timeout_context.result = execution_result;
    update_job_visibility_timeout_context.Finish();
    return;
  }

  update_job_visibility_timeout_context.response =
      make_shared<UpdateJobVisibilityTimeoutResponse>();
  update_job_visibility_timeout_context.result = SuccessExecutionResult();
  update_job_visibility_timeout_context.Finish();
}

ExecutionResult JobClientProvider::DeleteOrphanedJobMessage(
    AsyncContext<DeleteOrphanedJobMessageRequest,
                 DeleteOrphanedJobMessageResponse>&
        delete_orphaned_job_context) noexcept {
  const string& job_id = delete_orphaned_job_context.request->job_id();
  if (job_id.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_MISSING_JOB_ID);
    SCP_ERROR_CONTEXT(kJobClientProvider, delete_orphaned_job_context,
                      execution_result,
                      "Failed to delete orphaned job due to missing job id.");
    delete_orphaned_job_context.result = execution_result;
    delete_orphaned_job_context.Finish();
    return execution_result;
  }

  const string& receipt_info =
      delete_orphaned_job_context.request->receipt_info();
  if (receipt_info.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_RECEIPT_INFO);
    SCP_ERROR_CONTEXT(kJobClientProvider, delete_orphaned_job_context,
                      execution_result,
                      "Failed to delete orphaned job due to missing receipt "
                      "info in the request. Job id: %s",
                      job_id.c_str());
    delete_orphaned_job_context.result = execution_result;
    delete_orphaned_job_context.Finish();
    return execution_result;
  }

  auto get_database_item_request =
      JobClientUtils::CreateGetJobByJobIdRequest(job_table_name_, job_id);
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      get_database_item_context(
          move(get_database_item_request),
          bind(&JobClientProvider::
                   OnGetJobItemForDeleteOrphanedJobMessageCallback,
               this, delete_orphaned_job_context, _1),
          delete_orphaned_job_context);

  return nosql_database_client_provider_->GetDatabaseItem(
      get_database_item_context);
}

void JobClientProvider::OnGetJobItemForDeleteOrphanedJobMessageCallback(
    AsyncContext<DeleteOrphanedJobMessageRequest,
                 DeleteOrphanedJobMessageResponse>& delete_orphaned_job_context,
    AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>&
        get_database_item_context) noexcept {
  const string& job_id = delete_orphaned_job_context.request->job_id();
  if (!get_database_item_context.result.Successful()) {
    if (get_database_item_context.result.status_code ==
        SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND) {
      DeleteJobMessageForDeletingOrphanedJob(delete_orphaned_job_context);
      return;
    } else {
      auto execution_result = get_database_item_context.result;
      SCP_ERROR_CONTEXT(kJobClientProvider, delete_orphaned_job_context,
                        execution_result,
                        "Failed to delete orphaned job due to get job from "
                        "NoSQL database failed. Job id: %s",
                        job_id.c_str());
      delete_orphaned_job_context.result = execution_result;
      delete_orphaned_job_context.Finish();
      return;
    }
  }
  const auto& item = get_database_item_context.response->item();
  auto job_or = JobClientUtils::ConvertDatabaseItemToJob(item);
  if (!job_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kJobClientProvider, delete_orphaned_job_context, job_or.result(),
        "Cannot convert database item to job. Job id: %s", job_id.c_str());
    delete_orphaned_job_context.result = job_or.result();
    delete_orphaned_job_context.Finish();
    return;
  }
  const auto& job_status = job_or->job_status();
  if (job_status == JobStatus::JOB_STATUS_SUCCESS ||
      job_status == JobStatus::JOB_STATUS_FAILURE) {
    DeleteJobMessageForDeletingOrphanedJob(delete_orphaned_job_context);
    return;
  }
  auto execution_result =
      FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS);
  SCP_ERROR_CONTEXT(kJobClientProvider, delete_orphaned_job_context,
                    execution_result,
                    "Failed to delete orphaned job due to the job status "
                    "is not in finished state. Job id: %s, job status: %s",
                    job_id.c_str(), job_status);
  delete_orphaned_job_context.result = execution_result;
  delete_orphaned_job_context.Finish();
  return;
}

void JobClientProvider::DeleteJobMessageForDeletingOrphanedJob(
    AsyncContext<DeleteOrphanedJobMessageRequest,
                 DeleteOrphanedJobMessageResponse>&
        delete_orphaned_job_context) noexcept {
  auto delete_message_request = make_shared<DeleteMessageRequest>();
  delete_message_request->set_allocated_receipt_info(
      delete_orphaned_job_context.request->release_receipt_info());
  AsyncContext<DeleteMessageRequest, DeleteMessageResponse>
      delete_message_context(
          move(delete_message_request),
          bind(&JobClientProvider::
                   OnDeleteJobMessageForDeleteOrphanedJobMessageCallback,
               this, delete_orphaned_job_context, _1),
          delete_orphaned_job_context);

  auto execution_result =
      queue_client_provider_->DeleteMessage(delete_message_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kJobClientProvider, delete_orphaned_job_context,
                      execution_result,
                      "Cannot delete message from queue. Job id: %s",
                      delete_orphaned_job_context.request->job_id().c_str());
    delete_orphaned_job_context.result = execution_result;
    delete_orphaned_job_context.Finish();
    return;
  }
}

void JobClientProvider::OnDeleteJobMessageForDeleteOrphanedJobMessageCallback(
    AsyncContext<DeleteOrphanedJobMessageRequest,
                 DeleteOrphanedJobMessageResponse>& delete_orphaned_job_context,
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_messasge_context) noexcept {
  const string& job_id = delete_orphaned_job_context.request->job_id();
  if (!delete_messasge_context.result.Successful()) {
    auto execution_result = delete_messasge_context.result;
    SCP_ERROR_CONTEXT(kJobClientProvider, delete_orphaned_job_context,
                      execution_result,
                      "Failed to delete orphaned job due to job message "
                      "deletion failed. Job id; %s",
                      job_id.c_str());
    delete_orphaned_job_context.result = execution_result;
    delete_orphaned_job_context.Finish();
    return;
  }

  delete_orphaned_job_context.response =
      make_shared<DeleteOrphanedJobMessageResponse>();
  delete_orphaned_job_context.result = SuccessExecutionResult();
  delete_orphaned_job_context.Finish();
}

}  // namespace google::scp::cpio::client_providers
