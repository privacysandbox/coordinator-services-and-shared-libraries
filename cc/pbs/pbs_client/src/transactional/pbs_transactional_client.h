// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/credentials_provider_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/journal_service_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/interface/token_provider_cache_interface.h"
#include "core/interface/transaction_command_serializer_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/interface/pbs_client_interface.h"
#include "pbs/interface/pbs_transactional_client_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {
/*! @copydoc PrivacyBudgetServiceTransactionalClientInterface
 */
class PrivacyBudgetServiceTransactionalClient
    : public PrivacyBudgetServiceTransactionalClientInterface {
 public:
  /**
   * @brief Construct a new Privacy Budget Service Transactional Client object
   *
   * @param reporting_origin
   * @param pbs_endpoint
   * @param http_client
   * @param async_executor
   * @param pbs_auth_token_cache
   */
  PrivacyBudgetServiceTransactionalClient(
      const std::string& reporting_origin, const std::string& pbs_endpoint,
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::TokenProviderCacheInterface>&
          authorization_token_provider_cache);

  /**
   * @brief Construct a new Privacy Budget Service Transactional Client object
   *
   * @param reporting_origin
   * @param pbs1_endpoint
   * @param pbs2_endpoint
   * @param http_client
   * @param async_executor
   * @param pbs1_auth_token_cache
   * @param pbs2_auth_token_cache
   */
  PrivacyBudgetServiceTransactionalClient(
      const std::string& reporting_origin, const std::string& pbs1_endpoint,
      const std::string& pbs2_endpoint,
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::TokenProviderCacheInterface>&
          pbs1_auth_token_cache,
      const std::shared_ptr<core::TokenProviderCacheInterface>&
          pbs2_auth_token_cache);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult ConsumeBudget(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context) noexcept override;

  core::ExecutionResult GetTransactionStatusOnPBS1(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>
          get_transaction_status_context) noexcept override;

  core::ExecutionResult GetTransactionStatusOnPBS2(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>
          get_transaction_status_context) noexcept override;

 protected:
  PrivacyBudgetServiceTransactionalClient(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::HttpClientInterface>& http_client);

  void OnConsumeBudgetCallback(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context,
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          transaction_context) noexcept;

  /// Indicates whether this is a single coordinator client.
  bool is_single_coordinator_mode;
  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  /// The http client to call the endpoints.
  std::shared_ptr<core::HttpClientInterface> http_client_;
  /// The first privacy budget service client.
  std::shared_ptr<PrivacyBudgetServiceClientInterface> pbs1_client_;
  /// The second privacy budget service client.
  std::shared_ptr<PrivacyBudgetServiceClientInterface> pbs2_client_;
  /// The max number of concurrent transactions.
  size_t max_concurrent_transactions_;
  /// An instance of the transaction command serializer.
  std::shared_ptr<core::TransactionCommandSerializerInterface>
      transaction_command_serializer_;
  /// An instance of the journal service.
  std::shared_ptr<core::JournalServiceInterface> journal_service_;
  /// An instance of the remote transaction manager.
  std::shared_ptr<core::RemoteTransactionManagerInterface>
      remote_transaction_manager_;
  /// An instance of the metric client.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;
  /// Config provider
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  /// An instance of the transaction manager.
  std::shared_ptr<core::TransactionManagerInterface> transaction_manager_;
};
}  // namespace google::scp::pbs
