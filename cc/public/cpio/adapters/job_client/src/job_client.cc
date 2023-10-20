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

#include "job_client.h"

#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"

using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageRequest;
using google::cmrt::sdk::job_service::v1::DeleteOrphanedJobMessageResponse;
using google::cmrt::sdk::job_service::v1::GetJobByIdRequest;
using google::cmrt::sdk::job_service::v1::GetJobByIdResponse;
using google::cmrt::sdk::job_service::v1::GetNextJobRequest;
using google::cmrt::sdk::job_service::v1::GetNextJobResponse;
using google::cmrt::sdk::job_service::v1::PutJobRequest;
using google::cmrt::sdk::job_service::v1::PutJobResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobBodyResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobStatusResponse;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutRequest;
using google::cmrt::sdk::job_service::v1::UpdateJobVisibilityTimeoutResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::JobClientProviderFactory;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

namespace {
constexpr char kJobClient[] = "JobClient";
}  // namespace

namespace google::scp::cpio {

ExecutionResult JobClient::Init() noexcept {
  shared_ptr<InstanceClientProviderInterface> instance_client;
  auto execution_result =
      GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(instance_client);
  if (!execution_result.Successful()) {
    SCP_ERROR(kJobClient, kZeroUuid, execution_result,
              "Failed to get InstanceClientProvider.");
    return execution_result;
  }

  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  execution_result =
      GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(cpu_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kJobClient, kZeroUuid, execution_result,
              "Failed to get CpuAsyncExecutor.");
    return execution_result;
  }

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  execution_result =
      GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(io_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kJobClient, kZeroUuid, execution_result,
              "Failed to get IoAsyncExecutor.");
    return execution_result;
  }

  job_client_provider_ = JobClientProviderFactory::Create(
      options_, instance_client, cpu_async_executor, io_async_executor);
  execution_result = job_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kJobClient, kZeroUuid, execution_result,
              "Failed to initialize JobClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult JobClient::Run() noexcept {
  auto execution_result = job_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kJobClient, kZeroUuid, execution_result,
              "Failed to run JobClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult JobClient::Stop() noexcept {
  auto execution_result = job_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kJobClient, kZeroUuid, execution_result,
              "Failed to stop JobClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult JobClient::PutJob(
    AsyncContext<PutJobRequest, PutJobResponse>& put_job_context) noexcept {
  return job_client_provider_->PutJob(put_job_context);
}

ExecutionResult JobClient::GetNextJob(
    AsyncContext<GetNextJobRequest, GetNextJobResponse>&
        get_next_job_context) noexcept {
  return job_client_provider_->GetNextJob(get_next_job_context);
}

ExecutionResult JobClient::GetJobById(
    AsyncContext<GetJobByIdRequest, GetJobByIdResponse>&
        get_job_by_id_context) noexcept {
  return job_client_provider_->GetJobById(get_job_by_id_context);
};

ExecutionResult JobClient::UpdateJobBody(
    AsyncContext<UpdateJobBodyRequest, UpdateJobBodyResponse>&
        update_job_body_context) noexcept {
  return job_client_provider_->UpdateJobBody(update_job_body_context);
};

ExecutionResult JobClient::UpdateJobStatus(
    AsyncContext<UpdateJobStatusRequest, UpdateJobStatusResponse>&
        update_job_status_context) noexcept {
  return job_client_provider_->UpdateJobStatus(update_job_status_context);
};

ExecutionResult JobClient::UpdateJobVisibilityTimeout(
    AsyncContext<UpdateJobVisibilityTimeoutRequest,
                 UpdateJobVisibilityTimeoutResponse>&
        update_job_visibility_timeout_context) noexcept {
  return job_client_provider_->UpdateJobVisibilityTimeout(
      update_job_visibility_timeout_context);
};

ExecutionResult JobClient::DeleteOrphanedJobMessage(
    AsyncContext<DeleteOrphanedJobMessageRequest,
                 DeleteOrphanedJobMessageResponse>&
        delete_orphaned_job_context) noexcept {
  return job_client_provider_->DeleteOrphanedJobMessage(
      delete_orphaned_job_context);
};

unique_ptr<JobClientInterface> JobClientFactory::Create(
    JobClientOptions options) noexcept {
  return make_unique<JobClient>(make_shared<JobClientOptions>(options));
}
}  // namespace google::scp::cpio
