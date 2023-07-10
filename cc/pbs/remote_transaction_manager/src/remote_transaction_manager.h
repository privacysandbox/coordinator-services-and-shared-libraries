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

#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/credentials_provider_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/interface/type_def.h"

namespace google::scp::pbs {
/*! @copydoc RemoteTransactionManagerInterface
 */
class RemoteTransactionManager
    : public core::RemoteTransactionManagerInterface {
 public:
  RemoteTransactionManager(
      const std::shared_ptr<PrivacyBudgetServiceClientInterface>& pbs_client);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override;

  core::ExecutionResult ExecutePhase(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context) noexcept override;

 protected:
  /// An instance of the PBS client.
  std::shared_ptr<PrivacyBudgetServiceClientInterface> pbs_client_;
};
}  // namespace google::scp::pbs
