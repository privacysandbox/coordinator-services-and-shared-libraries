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
#include "cpio/client_providers/interface/job_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/job_client/job_client_interface.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc JobClientInterface
 */
class JobClient : public JobClientInterface {
 public:
  explicit JobClient(const std::shared_ptr<JobClientOptions>& options)
      : options_(options) {}

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

 protected:
  std::shared_ptr<client_providers::JobClientProviderInterface>
      job_client_provider_;

 private:
  std::shared_ptr<JobClientOptions> options_;
};
}  // namespace google::scp::cpio
