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

#include "pbs_transactional_client.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/authorization_service_interface.h"
#include "core/journal_service/mock/mock_journal_service.h"
#include "core/transaction_manager/mock/mock_transaction_command_serializer.h"
#include "core/transaction_manager/src/transaction_manager.h"
#include "pbs/pbs_client/src/pbs_client.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

#include "client_consume_budget_command.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionManager;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::journal_service::mock::MockJournalService;
using google::scp::core::transaction_manager::mock::
    MockTransactionCommandSerializer;
using google::scp::cpio::MockMetricClient;
using std::bind;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;

namespace google::scp::pbs {
PrivacyBudgetServiceTransactionalClient::
    PrivacyBudgetServiceTransactionalClient(
        const std::string& reporting_origin, const std::string& pbs_endpoint,
        const shared_ptr<HttpClientInterface>& http_client,
        const shared_ptr<AsyncExecutorInterface>& async_executor,
        const shared_ptr<TokenProviderCacheInterface>&
            authorization_token_provider_cache)
    : PrivacyBudgetServiceTransactionalClient(async_executor, http_client) {
  pbs1_client_ = make_shared<PrivacyBudgetServiceClient>(
      reporting_origin, pbs_endpoint, http_client_,
      authorization_token_provider_cache);
  is_single_coordinator_mode = true;
}

PrivacyBudgetServiceTransactionalClient::
    PrivacyBudgetServiceTransactionalClient(
        const string& reporting_origin, const string& pbs1_endpoint,
        const string& pbs2_endpoint,
        const shared_ptr<HttpClientInterface>& http_client,
        const shared_ptr<AsyncExecutorInterface>& async_executor,
        const shared_ptr<TokenProviderCacheInterface>& pbs1_auth_token_cache,
        const shared_ptr<TokenProviderCacheInterface>& pbs2_auth_token_cache)
    : PrivacyBudgetServiceTransactionalClient(async_executor, http_client) {
  pbs1_client_ = make_shared<PrivacyBudgetServiceClient>(
      reporting_origin, pbs1_endpoint, http_client_, pbs1_auth_token_cache);
  pbs2_client_ = make_shared<PrivacyBudgetServiceClient>(
      reporting_origin, pbs2_endpoint, http_client_, pbs2_auth_token_cache);
  is_single_coordinator_mode = false;
}

PrivacyBudgetServiceTransactionalClient::
    PrivacyBudgetServiceTransactionalClient(
        const shared_ptr<AsyncExecutorInterface>& async_executor,
        const shared_ptr<HttpClientInterface>& http_client)
    : is_single_coordinator_mode(false),
      async_executor_(async_executor),
      http_client_(http_client),
      max_concurrent_transactions_(100000),
      transaction_command_serializer_(
          make_shared<MockTransactionCommandSerializer>()),
      journal_service_(make_shared<MockJournalService>()),
      metric_client_(make_shared<MockMetricClient>()),
      config_provider_(make_shared<MockConfigProvider>()),
      transaction_manager_(make_shared<TransactionManager>(
          async_executor_, transaction_command_serializer_, journal_service_,
          remote_transaction_manager_, max_concurrent_transactions_,
          metric_client_, config_provider_)) {
  auto mock_metric_client =
      dynamic_pointer_cast<MockMetricClient>(metric_client_);
  ON_CALL(*mock_metric_client, PutMetrics).WillByDefault([&](auto context) {
    context.result = SuccessExecutionResult();
    context.Finish();
    return SuccessExecutionResult();
  });
}

ExecutionResult PrivacyBudgetServiceTransactionalClient::Init() noexcept {
  auto execution_result = pbs1_client_->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (!is_single_coordinator_mode) {
    execution_result = pbs2_client_->Init();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return transaction_manager_->Init();
}

ExecutionResult PrivacyBudgetServiceTransactionalClient::Run() noexcept {
  auto execution_result = pbs1_client_->Run();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (!is_single_coordinator_mode) {
    execution_result = pbs2_client_->Run();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return transaction_manager_->Run();
}

ExecutionResult PrivacyBudgetServiceTransactionalClient::Stop() noexcept {
  auto execution_result = pbs1_client_->Stop();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  if (!is_single_coordinator_mode) {
    execution_result = pbs2_client_->Stop();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  execution_result = transaction_manager_->Stop();
  if (!execution_result.Successful()) {
    return execution_result;
  }

  return execution_result;
}

ExecutionResult PrivacyBudgetServiceTransactionalClient::ConsumeBudget(
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>&
        consume_budget_transaction_context) noexcept {
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context(
      make_shared<TransactionRequest>(),
      bind(&PrivacyBudgetServiceTransactionalClient::OnConsumeBudgetCallback,
           this, consume_budget_transaction_context, _1),
      consume_budget_transaction_context);

  transaction_context.request->is_coordinated_remotely = false;
  transaction_context.request->transaction_id =
      consume_budget_transaction_context.request->transaction_id;
  transaction_context.request->transaction_secret =
      consume_budget_transaction_context.request->transaction_secret;

  transaction_context.request->commands.push_back(
      make_shared<ClientConsumeBudgetCommand>(
          transaction_context.request->transaction_id,
          transaction_context.request->transaction_secret,
          consume_budget_transaction_context.request->budget_keys,
          async_executor_, pbs1_client_,
          consume_budget_transaction_context.activity_id));

  if (!is_single_coordinator_mode) {
    transaction_context.request->commands.push_back(
        make_shared<ClientConsumeBudgetCommand>(
            transaction_context.request->transaction_id,
            transaction_context.request->transaction_secret,
            consume_budget_transaction_context.request->budget_keys,
            async_executor_, pbs2_client_,
            consume_budget_transaction_context.activity_id));
  }

  return transaction_manager_->Execute(transaction_context);
}

ExecutionResult
PrivacyBudgetServiceTransactionalClient::GetTransactionStatusOnPBS1(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
        context) noexcept {
  return pbs1_client_->GetTransactionStatus(context);
}

ExecutionResult
PrivacyBudgetServiceTransactionalClient::GetTransactionStatusOnPBS2(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
        context) noexcept {
  return pbs2_client_->GetTransactionStatus(context);
}

void PrivacyBudgetServiceTransactionalClient::OnConsumeBudgetCallback(
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>&
        consume_budget_transaction_context,
    AsyncContext<TransactionRequest, TransactionResponse>&
        transaction_context) noexcept {
  consume_budget_transaction_context.result = transaction_context.result;
  consume_budget_transaction_context.Finish();
}

}  // namespace google::scp::pbs
