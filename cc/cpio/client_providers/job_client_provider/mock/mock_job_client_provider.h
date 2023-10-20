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

#include <gmock/gmock.h>

#include <memory>

#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "cpio/client_providers/job_client_provider/src/job_client_provider.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockJobClientProvider : public JobClientProviderInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  MOCK_METHOD(
      core::ExecutionResult, PutJob,
      ((core::AsyncContext<cmrt::sdk::job_service::v1::PutJobRequest,
                           cmrt::sdk::job_service::v1::PutJobResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, GetNextJob,
      ((core::AsyncContext<cmrt::sdk::job_service::v1::GetNextJobRequest,
                           cmrt::sdk::job_service::v1::GetNextJobResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, GetJobById,
      ((core::AsyncContext<cmrt::sdk::job_service::v1::GetJobByIdRequest,
                           cmrt::sdk::job_service::v1::GetJobByIdResponse>&)),
      (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, UpdateJobBody,
              ((core::AsyncContext<
                  cmrt::sdk::job_service::v1::UpdateJobBodyRequest,
                  cmrt::sdk::job_service::v1::UpdateJobBodyResponse>&)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, UpdateJobStatus,
              ((core::AsyncContext<
                  cmrt::sdk::job_service::v1::UpdateJobStatusRequest,
                  cmrt::sdk::job_service::v1::UpdateJobStatusResponse>&)),
              (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, UpdateJobVisibilityTimeout,
      ((core::AsyncContext<
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest,
          cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse>&)),
      (noexcept, override));

  MOCK_METHOD(
      core::ExecutionResult, DeleteOrphanedJobMessage,
      ((core::AsyncContext<
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest,
          cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse>&)),
      (noexcept, override));

  core::ExecutionResult ConvertDatabaseErrorForPutJob(
      const core::StatusCode status_code_from_database) noexcept {
    return google::scp::core::FailureExecutionResult(status_code_from_database);
  };
};
}  // namespace google::scp::cpio::client_providers::mock
