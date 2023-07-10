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

#include <functional>
#include <memory>
#include <string>

#include "pbs/pbs_client/src/pbs_client.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::client::mock {
class MockPrivacyBudgetServiceClientWithOverrides
    : public PrivacyBudgetServiceClient {
 public:
  MockPrivacyBudgetServiceClientWithOverrides(
      const std::string& reporting_origin, const std::string& pbs_endpoint,
      const std::shared_ptr<core::HttpClientInterface>& http_client,
      const std::shared_ptr<core::TokenProviderCacheInterface>&
          authorization_token_provider_cache)
      : PrivacyBudgetServiceClient(reporting_origin, pbs_endpoint, http_client,
                                   authorization_token_provider_cache) {}

  virtual void OnInitiateConsumeBudgetTransactionCallback(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    PrivacyBudgetServiceClient::OnInitiateConsumeBudgetTransactionCallback(
        consume_budget_transaction_context, http_context);
  }

  virtual void OnExecuteTransactionPhaseCallback(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    PrivacyBudgetServiceClient::OnExecuteTransactionPhaseCallback(
        transaction_phase_context, http_context);
  }

  virtual void OnGetTransactionStatusCallback(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    PrivacyBudgetServiceClient::OnGetTransactionStatusCallback(
        get_transaction_status_context, http_context);
  }

  std::string GetTransactionStatusUrl() { return *get_transaction_status_url_; }

  std::string GetExecuteTransactionBeginPhaseUrl() {
    return *begin_consume_budget_transaction_url_;
  }

  std::string GetExecuteTransactionPreparePhaseUrl() {
    return *prepare_consume_budget_transaction_url_;
  }

  std::string GetExecuteTransactionCommitPhaseUrl() {
    return *commit_consume_budget_transaction_url_;
  }

  std::string GetExecuteTransactionNotifyPhaseUrl() {
    return *notify_consume_budget_transaction_url_;
  }

  std::string GetExecuteTransactionAbortPhaseUrl() {
    return *abort_consume_budget_transaction_url_;
  }

  std::string GetExecuteTransactionEndPhaseUrl() {
    return *end_consume_budget_transaction_url_;
  }
};
}  // namespace google::scp::pbs::client::mock
