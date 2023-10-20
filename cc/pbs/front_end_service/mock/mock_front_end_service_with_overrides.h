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

#include "pbs/front_end_service/src/front_end_service.h"
#include "public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

namespace google::scp::pbs::front_end_service::mock {

class MockFrontEndServiceWithOverrides : public FrontEndService {
 public:
  MockFrontEndServiceWithOverrides(
      std::shared_ptr<core::HttpServerInterface>& http_server,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::unique_ptr<core::TransactionRequestRouterInterface>
          transaction_request_router,
      std::unique_ptr<ConsumeBudgetCommandFactoryInterface> command_factory,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : FrontEndService(
            http_server, async_executor, std::move(transaction_request_router),
            std::move(command_factory), metric_client, config_provider) {
    remote_coordinator_claimed_identity_ = "remote-coordinator.com";
  }

  std::function<core::ExecutionResult(
      const std::shared_ptr<cpio::AggregateMetricInterface>&,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&,
      core::common::Uuid&, std::shared_ptr<std::string>& transaction_secret,
      std::shared_ptr<std::string>& transaction_origin, core::Timestamp,
      core::TransactionExecutionPhase)>
      execution_transaction_phase_mock;

  core::ExecutionResultOr<std::shared_ptr<cpio::AggregateMetricInterface>>
  RegisterAggregateMetric(const std::string& name,
                          const std::string& phase) noexcept {
    std::shared_ptr<cpio::AggregateMetricInterface> metrics_instance =
        std::make_shared<cpio::MockAggregateMetric>();
    return metrics_instance;
  }

  core::ExecutionResult InitMetricInstances() noexcept {
    return FrontEndService::InitMetricInstances();
  }

  void OnTransactionCallback(
      const std::shared_ptr<cpio::AggregateMetricInterface>& metric_instance,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          transaction_context) noexcept {
    FrontEndService::OnTransactionCallback(metric_instance, http_context,
                                           transaction_context);
  }

  std::shared_ptr<cpio::MockAggregateMetric> GetMetricsInstance(
      const std::string& method_name, const std::string& phase) {
    std::shared_ptr<cpio::AggregateMetricInterface> metrics_instance =
        metrics_instances_map_.at(method_name).at(phase);
    return std::dynamic_pointer_cast<cpio::MockAggregateMetric>(
        metrics_instance);
  }

  core::ExecutionResult ExecuteTransactionPhase(
      const std::shared_ptr<cpio::AggregateMetricInterface>& metric_instance,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::common::Uuid& transaction_id,
      std::shared_ptr<std::string>& transaction_secret,
      std::shared_ptr<std::string>& transaction_origin,
      core::Timestamp last_transaction_execution_timestamp,
      core::TransactionExecutionPhase transaction_phase) noexcept override {
    if (execution_transaction_phase_mock) {
      return execution_transaction_phase_mock(
          metric_instance, http_context, transaction_id, transaction_secret,
          transaction_origin, last_transaction_execution_timestamp,
          transaction_phase);
    }

    return FrontEndService::ExecuteTransactionPhase(
        metric_instance, http_context, transaction_id, transaction_secret,
        transaction_origin, last_transaction_execution_timestamp,
        transaction_phase);
  }

  void OnExecuteTransactionPhaseCallback(
      const std::shared_ptr<cpio::AggregateMetricInterface>& metric_instance,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context) noexcept {
    FrontEndService::OnExecuteTransactionPhaseCallback(
        metric_instance, http_context, transaction_phase_context);
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
      const std::shared_ptr<cpio::AggregateMetricInterface>& metric_instance,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept {
    FrontEndService::OnGetTransactionStatusCallback(
        metric_instance, http_context, get_transaction_status_context);
  }

  std::vector<std::shared_ptr<core::TransactionCommand>>
  GenerateConsumeBudgetCommands(
      std::list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
      const std::string& authorized_domain,
      const core::common::Uuid& transaction_id) {
    return FrontEndService::GenerateConsumeBudgetCommands(
        consume_budget_metadata_list, authorized_domain, transaction_id);
  }

  std::vector<std::shared_ptr<core::TransactionCommand>>
  GenerateConsumeBudgetCommandsWithBatchesPerDay(
      std::list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
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
