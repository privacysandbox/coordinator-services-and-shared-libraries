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
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "core/interface/async_executor_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/token_provider_cache_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/interface/pbs_client_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs {
/*! @copydoc PrivacyBudgetServiceClientInterface
 */
class PrivacyBudgetServiceClient : public PrivacyBudgetServiceClientInterface {
 public:
  /**
   * @brief Constructs a new Privacy Budget Service Client object
   *
   * @param reporting_origin The reporting origin of the caller.
   * @param pbs_endpoint The privacy budget service endpoint.
   * @param http_client The http client to be used for contacting the privacy
   * budget service.
   * @param authorization_token_provider_cache The authorization token provider
   * cache to get token from for HTTP requests.
   */
  PrivacyBudgetServiceClient(
      const std::string& reporting_origin, const std::string& pbs_endpoint,
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<core::TokenProviderCacheInterface>&
          authorization_token_provider_cache);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override;

  core::ExecutionResult InitiateConsumeBudgetTransaction(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context) noexcept override;

  core::ExecutionResult ExecuteTransactionPhase(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context) noexcept override;

 protected:
  /**
   * @brief Is called when the get transaction status operation completes.
   *
   * @param get_transaction_status_context The get transaction status
   * context of the operation.
   * @param http_context The http context of the http operation.
   */
  virtual void OnGetTransactionStatusCallback(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Serializes consume budget transaction request to used for the http
   * request.
   *
   * @param consume_budget_transaction_request Consume budget transaction
   * request.
   * @param serialized The serialized value.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult SerializeConsumeBudgetTransactionRequest(
      std::shared_ptr<ConsumeBudgetTransactionRequest>&
          consume_budget_transaction_request,
      std::string& serialized) noexcept;

  /**
   * @brief Is called when the consume budget transaction operation completes.
   *
   * @param consume_budget_transaction_context The consume budget transaction
   * context of the operation.
   * @param http_context The http context of the http operation.
   */
  virtual void OnInitiateConsumeBudgetTransactionCallback(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Is called when the execute transaction phase operation completes.
   *
   * @param transaction_phase_context The transaction phase context of the
   * operation.
   * @param http_context The http context of the http operation.
   */
  virtual void OnExecuteTransactionPhaseCallback(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  core::ExecutionResult ParseTransactionLastExecutionTime(
      std::string& transaction_last_execution_time_str,
      core::Timestamp& transaction_last_execution_time) noexcept;

 protected:
  /// The pre-constructed get transaction status  url.
  std::shared_ptr<std::string> get_transaction_status_url_;
  /// The pre-constructed begin consume budget transaction url.
  std::shared_ptr<std::string> begin_consume_budget_transaction_url_;
  /// The pre-constructed prepare consume budget transaction  url.
  std::shared_ptr<std::string> prepare_consume_budget_transaction_url_;
  /// The pre-constructed commit consume budget transaction  url.
  std::shared_ptr<std::string> commit_consume_budget_transaction_url_;
  /// The pre-constructed notify consume budget transaction  url.
  std::shared_ptr<std::string> notify_consume_budget_transaction_url_;
  /// The pre-constructed abort consume budget transaction  url.
  std::shared_ptr<std::string> abort_consume_budget_transaction_url_;
  /// The pre-constructed end consume budget transaction  url.
  std::shared_ptr<std::string> end_consume_budget_transaction_url_;

 private:
  /// The reporting origin
  const std::string reporting_origin_;
  /// The privacy budget service endpoint.
  const std::string pbs_endpoint_;
  /// The http client to use for the http operations.
  const std::shared_ptr<core::HttpClientInterface> http_client_;
  /// The auth token provider cache
  const std::shared_ptr<core::TokenProviderCacheInterface>
      authorization_token_provider_cache_;
};
}  // namespace google::scp::pbs
