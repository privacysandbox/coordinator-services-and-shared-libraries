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

#include "gcp_role_credentials_provider.h"

#include <memory>

#include "cpio/client_providers/interface/role_credentials_provider_interface.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio::client_providers {
ExecutionResult GcpRoleCredentialsProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpRoleCredentialsProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpRoleCredentialsProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpRoleCredentialsProvider::GetRoleCredentials(
    core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context) noexcept {
  return FailureExecutionResult(SC_UNKNOWN);
}

shared_ptr<RoleCredentialsProviderInterface>
RoleCredentialsProviderFactory::Create(
    const shared_ptr<RoleCredentialsProviderOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<core::AsyncExecutorInterface>&
        io_async_executor) noexcept {
  return make_shared<GcpRoleCredentialsProvider>();
}
}  // namespace google::scp::cpio::client_providers
