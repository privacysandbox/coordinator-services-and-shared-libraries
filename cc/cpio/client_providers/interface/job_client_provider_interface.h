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

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/job_client/type_def.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"

namespace google::scp::cpio::client_providers {

/**
 * @brief Interface responsible for storing and fetching jobs.
 */
class JobClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~JobClientProviderInterface() = default;
  /**
   * @brief Put a Job.
   * @param put_job_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult PutJob(
      core::AsyncContext<cmrt::sdk::job_service::v1::PutJobRequest,
                         cmrt::sdk::job_service::v1::PutJobResponse>&
          put_job_context) noexcept = 0;
  /**
   * @brief Get the first available Job.
   * @param get_next_job_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetNextJob(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                         cmrt::sdk::job_service::v1::GetNextJobResponse>&
          get_next_job_context) noexcept = 0;
  /**
   * @brief Get a Job by job id.
   * @param get_job_by_id_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetJobById(
      core::AsyncContext<cmrt::sdk::job_service::v1::GetJobByIdRequest,
                         cmrt::sdk::job_service::v1::GetJobByIdResponse>&
          get_job_by_id_context) noexcept = 0;
  /**
   * @brief Update job body of a Job.
   * @param update_job_body_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult UpdateJobBody(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobBodyRequest,
                         cmrt::sdk::job_service::v1::UpdateJobBodyResponse>&
          update_job_body_context) noexcept = 0;
  /**
   * @brief Update status of a Job.
   * @param update_job_status_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult UpdateJobStatus(
      core::AsyncContext<cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                         cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&
          update_job_status_context) noexcept = 0;

  /**
   * @brief Update visibility timeout of a Job.
   * @param update_job_visibility_timeout_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult UpdateJobVisibilityTimeout(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest,
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>&
          update_job_visibility_timeout_context) noexcept = 0;

  /**
   * @brief Deletes the orphaned job from the job queue.
   * @param delete_orphaned_job_context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult DeleteOrphanedJobMessage(
      core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&
          delete_orphaned_job_context) noexcept = 0;
};

class JobClientProviderFactory {
 public:
  /**
   * @brief Factory to create JobClientProvider.
   *
   * @param options JobClientOptions.
   * @param instance_client Instance Client.
   * @param cpu_async_executor CPU Async Eexcutor.
   * @param io_async_executor IO Async Eexcutor.
   * @return std::shared_ptr<JobClientProviderInterface> created
   * JobClientProviderProvider.
   */
  static std::shared_ptr<JobClientProviderInterface> Create(
      const std::shared_ptr<JobClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface> instance_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers
