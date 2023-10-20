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

#include <memory>
#include <string>
#include <utility>

#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/job_client_provider_interface.h"
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {

/*! @copydoc JobClientProviderInterface
 */
class JobClientProvider : public JobClientProviderInterface {
 public:
  virtual ~JobClientProvider() = default;

  explicit JobClientProvider(
      const std::shared_ptr<JobClientOptions>& job_client_options,
      const std::shared_ptr<QueueClientProviderInterface>&
          queue_client_provider,
      const std::shared_ptr<NoSQLDatabaseClientProviderInterface>&
          nosql_database_client_provider)
      : job_client_options_(job_client_options),
        queue_client_provider_(queue_client_provider),
        nosql_database_client_provider_(nosql_database_client_provider) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult PutJob(
      core::AsyncContext<cmrt::sdk::job_service::v1::PutJobRequest,
                         cmrt::sdk::job_service::v1::PutJobResponse>&
          put_job_context) noexcept override;

  core::ExecutionResult GetNextJob(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                         cmrt::sdk::job_service::v1::GetNextJobResponse>&
          get_next_job_context) noexcept override;

  core::ExecutionResult GetJobById(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetJobByIdRequest,
                         cmrt::sdk::job_service::v1::GetJobByIdResponse>&
          get_job_by_id_context) noexcept override;

  core::ExecutionResult UpdateJobBody(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobBodyRequest,
                         cmrt::sdk::job_service::v1::UpdateJobBodyResponse>&
          update_job_body_context) noexcept override;

  core::ExecutionResult UpdateJobStatus(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context) noexcept override;

  core::ExecutionResult UpdateJobVisibilityTimeout(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest,
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>&
          update_job_visibility_timeout_context) noexcept override;

  core::ExecutionResult DeleteOrphanedJobMessage(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&
          delete_orphaned_job_context) noexcept override;

 private:
  /**
   * @brief Is called when the object is returned from the enqueue message
   * callback.
   *
   * @param put_job_context the put job context.
   * @param enqueue_message_context the enqueue message context.
   */
  void OnEnqueueMessageCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::PutJobRequest,
                         cmrt::sdk::job_service::v1::PutJobResponse>&
          put_job_context,
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context) noexcept;

  /**
   * @brief Is called when the object is returned from the upsert new job item
   * into database callback.
   *
   * @param put_job_context the put job context.
   * @param job the job item created for response in put_job_context.
   * @param upsert_database_item_context the upsert database item context.
   */
  void OnUpsertNewJobItemCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::PutJobRequest,
                         cmrt::sdk::job_service::v1::PutJobResponse>&
          put_job_context,
      std::shared_ptr<cmrt::sdk::job_service::v1::Job> job,
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept;

  /**
   * @brief Is called when the object is returned from the get top message
   * callback.
   *
   * @param get_next_job_context the get next job context.
   * @param get_top_message_context the get top message context.
   */
  void OnGetTopMessageCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                         cmrt::sdk::job_service::v1::GetNextJobResponse>&
          get_next_job_context,
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context) noexcept;

  /**
   * @brief Is called when the object is returned from the get next job item
   * from database callback.
   *
   * @param get_next_job_context the get next job context.
   * @param receipt_info the receipt info of the job message from queue.
   * @param get_database_item_context the get database item context.
   */
  void OnGetNextJobItemCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                         cmrt::sdk::job_service::v1::GetNextJobResponse>&
          get_next_job_context,
      std::shared_ptr<std::string> receipt_info,
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context) noexcept;

  /**
   * @brief Is called when the object is returned from the get job item with job
   * id from database callback.
   *
   * @param get_job_by_id_context the get job by id context.
   * @param get_database_item_context the get database item context.
   */
  void OnGetJobItemByJobIdCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetJobByIdRequest,
                         cmrt::sdk::job_service::v1::GetJobByIdResponse>&
          get_job_by_id_context,
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context) noexcept;

  /**
   * @brief Is called when the object is returned from the get job item for
   * updating job body from database callback.
   *
   * @param update_job_body_context the update job body context.
   * @param get_database_item_context the get database item context.
   */
  void OnGetJobItemForUpdateJobBodyCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobBodyRequest,
                         cmrt::sdk::job_service::v1::UpdateJobBodyResponse>&
          update_job_body_context,
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context) noexcept;

  /**
   * @brief Is called when the object is returned from the upsert job item with
   * updated job body from database callback.
   *
   * @param update_job_body_context the update job body context.
   * @param update_job_body_time the time the job body is updated.
   * @param upsert_database_item_context the upsert database item context.
   */
  void OnUpsertUpdatedJobBodyJobItemCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobBodyRequest,
                         cmrt::sdk::job_service::v1::UpdateJobBodyResponse>&
          update_job_body_context,
      std::shared_ptr<google::protobuf::Timestamp> update_job_body_time,
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept;

  /**
   * @brief Is called when the object is returned from the get job item for
   * updating job status from database callback.
   *
   * @param update_job_visibility_timeout_context the update job body context.
   * @param get_database_item_context the get database item context.
   */
  void OnGetJobItemForUpdateJobStatusCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context,
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context) noexcept;

  /**
   * @brief Upsert job item with updated job status from database.
   *
   * @param update_job_status_context the update job status context.
   * @param retry_count the number of times the job has been attempted for
   * processing.
   */
  void UpsertUpdatedJobStatusJobItem(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context,
      const int retry_count) noexcept;

  /**
   * @brief Is called when the object is returned from the upsert job item with
   * updated job status from database callback.
   *
   * @param update_job_status_context the update job status context.
   * @param update_time the time when job status is updated.
   * @param retry_count the number of times the job has been attempted for
   * processing.
   * @param upsert_database_item_context the upsert database item context.
   */
  void OnUpsertUpdatedJobStatusJobItemCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context,
      std::shared_ptr<google::protobuf::Timestamp> update_time,
      const int retry_count,
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse>&
          upsert_database_item_context) noexcept;

  /**
   * @brief Delete job when updating to JOB_STATUS_SUCCEEDED or
   * JOB_STATUS_FAILED status.
   *
   * @param update_job_status_context the update job status context.
   * @param update_time the time when job status is updated.
   * @param retry_count the number of times the job has been attempted for
   * processing.
   */
  void DeleteJobMessageForUpdatingJobStatus(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context,
      std::shared_ptr<google::protobuf::Timestamp> update_time,
      const int retry_count) noexcept;

  /**
   * @brief Is called when the object is returned from the delete message
   * callback for updating to JOB_STATUS_SUCCEEDED or JOB_STATUS_FAILED status.
   *
   * @param update_job_status_context the update job status context.
   * @param update_time the time when job status is updated.
   * @param retry_count the number of times the job has been attempted for
   * processing.
   * @param delete_messasge_context the delete message context.
   */
  void OnDeleteJobMessageForUpdatingJobStatusCallback(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context,
      std::shared_ptr<google::protobuf::Timestamp> update_time,
      const int retry_count,
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_messasge_context) noexcept;

  /**
   * @brief Is called when the object is returned from the update message
   * visibility timeout callback.
   *
   * @param update_job_visibility_timeout_context the update job visibility
   * timeout context.
   * @param update_message_visibility_timeout_context the update message
   * visibility timeout context.
   */
  void OnUpdateMessageVisibilityTimeoutCallback(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest,
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>&
          update_job_visibility_timeout_context,
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>&
          update_message_visibility_timeout_context) noexcept;

  /**
   * @brief Is called when the object is returned from the get job item for
   * deleting orphaned job from database callback.
   *
   * @param delete_orphaned_job_context the delete orphaned job context.
   * @param get_database_item_context the get database item context.
   */
  void OnGetJobItemForDeleteOrphanedJobMessageCallback(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&
          delete_orphaned_job_context,
      core::AsyncContext<
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest,
          cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse>&
          get_database_item_context) noexcept;

  /**
   * @brief Delete job message when deleting orphaned job from database
   * callback.
   *
   * @param delete_orphaned_job_context the update job status context.
   * processing.
   */
  void DeleteJobMessageForDeletingOrphanedJob(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&
          delete_orphaned_job_context) noexcept;

  /**
   * @brief Is called when the object is returned from the delete orphaned job
   * message callback.
   *
   * @param delete_orphaned_job_context the delete orphaned job context.
   * @param delete_messasge_context the delete message context.
   */
  void OnDeleteJobMessageForDeleteOrphanedJobMessageCallback(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&
          delete_orphaned_job_context,
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_messasge_context) noexcept;

  /**
   * @brief Convert a database error into error for put job.
   *
   * @param status_code_from_database The status code of the database error.
   * @return core::ExecutionResult The corresponding PutJob error result.
   */
  virtual core::ExecutionResult ConvertDatabaseErrorForPutJob(
      const core::StatusCode status_code_from_database) noexcept = 0;

  /// The configuration for job client.
  std::shared_ptr<JobClientOptions> job_client_options_;

  /// The name of the table to store job data.
  std::string job_table_name_;

  /// The Queue client provider.
  std::shared_ptr<QueueClientProviderInterface> queue_client_provider_;

  /// The NoSQL database client provider.
  std::shared_ptr<NoSQLDatabaseClientProviderInterface>
      nosql_database_client_provider_;
};

}  // namespace google::scp::cpio::client_providers
