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

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cc/pbs/front_end_service/src/front_end_service.h"

namespace google::scp::pbs::front_end_service::mock {

class MockFrontEndServiceWithOverrides : public FrontEndService {
 public:
  MockFrontEndServiceWithOverrides(
      std::shared_ptr<core::HttpServerInterface>& http_server,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::unique_ptr<core::TransactionRequestRouterInterface>
          transaction_request_router,
      std::unique_ptr<ConsumeBudgetCommandFactoryInterface> command_factory,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : FrontEndService(http_server, async_executor,
                        std::move(transaction_request_router),
                        std::move(command_factory), config_provider) {
    remote_coordinator_claimed_identity_ = "remote-coordinator.com";
  }

  MockFrontEndServiceWithOverrides(
      std::shared_ptr<core::HttpServerInterface>& http_server,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::unique_ptr<core::TransactionRequestRouterInterface>
          transaction_request_router,
      std::unique_ptr<ConsumeBudgetCommandFactoryInterface> command_factory,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      std::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
          total_request_counter,
      std::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
          client_error_counter,
      std::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
          server_error_counter)
      : FrontEndService(
            http_server, async_executor, std::move(transaction_request_router),
            std::move(command_factory), config_provider,
            std::move(total_request_counter), std::move(client_error_counter),
            std::move(server_error_counter)) {
    remote_coordinator_claimed_identity_ = "remote-coordinator.com";
  }

  std::function<core::ExecutionResult(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&,
      core::common::Uuid&, std::shared_ptr<std::string>& transaction_secret,
      std::shared_ptr<std::string>& transaction_origin, core::Timestamp,
      core::TransactionExecutionPhase)>
      execution_transaction_phase_mock;

  core::ExecutionResult Init() noexcept { return FrontEndService::Init(); }

  void OnTransactionCallback(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          transaction_context) noexcept {
    FrontEndService::OnTransactionCallback(http_context, transaction_context);
  }

  core::ExecutionResult ExecuteTransactionPhase(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::common::Uuid& transaction_id,
      std::shared_ptr<std::string>& transaction_secret,
      std::shared_ptr<std::string>& transaction_origin,
      core::Timestamp last_transaction_execution_timestamp,
      core::TransactionExecutionPhase transaction_phase,
      const std::string& metric_label) noexcept override {
    if (execution_transaction_phase_mock) {
      return execution_transaction_phase_mock(
          http_context, transaction_id, transaction_secret, transaction_origin,
          last_transaction_execution_timestamp, transaction_phase);
    }

    return FrontEndService::ExecuteTransactionPhase(
        http_context, transaction_id, transaction_secret, transaction_origin,
        last_transaction_execution_timestamp, transaction_phase, metric_label);
  }

  void OnExecuteTransactionPhaseCallback(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context,
      const std::string& metric_label) noexcept {
    FrontEndService::OnExecuteTransactionPhaseCallback(
        http_context, transaction_phase_context, metric_label);
  }

  core::ExecutionResult BeginTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    return FrontEndService::BeginTransaction(http_context);
  }

  core::ExecutionResult PrepareTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    return FrontEndService::PrepareTransaction(http_context);
  }

  core::ExecutionResult CommitTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    return FrontEndService::CommitTransaction(http_context);
  }

  core::ExecutionResult NotifyTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    return FrontEndService::NotifyTransaction(http_context);
  }

  core::ExecutionResult AbortTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    return FrontEndService::AbortTransaction(http_context);
  }

  core::ExecutionResult EndTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    return FrontEndService::EndTransaction(http_context);
  }

  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    return FrontEndService::GetTransactionStatus(http_context);
  }

  core::ExecutionResult GetServiceStatus(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept {
    return FrontEndService::GetServiceStatus(http_context);
  }

  void OnGetTransactionStatusCallback(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context,
      const std::string& metric_label) noexcept {
    FrontEndService::OnGetTransactionStatusCallback(
        http_context, get_transaction_status_context, metric_label);
  }

  std::vector<std::shared_ptr<core::TransactionCommand>>
  GenerateConsumeBudgetCommands(
      std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list,
      const std::string& authorized_domain,
      const core::common::Uuid& transaction_id) {
    return FrontEndService::GenerateConsumeBudgetCommands(
        consume_budget_metadata_list, authorized_domain, transaction_id);
  }

  std::vector<std::shared_ptr<core::TransactionCommand>>
  GenerateConsumeBudgetCommandsWithBatchesPerDay(
      std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list,
      const std::string& authorized_domain,
      const core::common::Uuid& transaction_id) {
    return FrontEndService::GenerateConsumeBudgetCommandsWithBatchesPerDay(
        consume_budget_metadata_list, authorized_domain, transaction_id);
  }

  std::shared_ptr<std::string> ObtainTransactionOrigin(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context)
      const {
    return FrontEndService::ObtainTransactionOrigin(http_context);
  }
};
}  // namespace google::scp::pbs::front_end_service::mock
