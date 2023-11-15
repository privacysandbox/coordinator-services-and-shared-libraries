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

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/http_server_interface.h"
#include "core/interface/http_types.h"
#include "core/interface/transaction_request_router_interface.h"
#include "core/interface/type_def.h"
#include "pbs/interface/front_end_service_interface.h"
#include "pbs/transactions/src/consume_budget_command_factory_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

namespace google::scp::pbs {
/*! @copydoc FrontEndServiceInterface
 */
class FrontEndService : public FrontEndServiceInterface {
 public:
  FrontEndService(
      std::shared_ptr<core::HttpServerInterface>& http_server,
      std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      std::unique_ptr<core::TransactionRequestRouterInterface>
          transaction_request_router,
      std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
          consume_budget_command_factory,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider);

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  /**
   * @brief Execute a transaction with the transaction manager as the
   * end-to-end orchestrator for the transaction.
   *
   * @param consume_budget_transaction_context
   * @return core::ExecutionResult
   */
  core::ExecutionResult ExecuteConsumeBudgetTransaction(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context) noexcept override;

 protected:
  /**
   * @brief Registers a Aggregate Metric object
   *
   * @param metrics_instance The pointer of the aggregate metric object.
   * @param name The event/method name of the aggregate metric.
   * @param phase The transaction phase this aggregate metric pertains to
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResultOr<
      std::shared_ptr<cpio::AggregateMetricInterface>>
  RegisterAggregateMetric(const std::string& name,
                          const std::string& phase) noexcept;

  /**
   * @brief Initializes the TransactionMetrics instances for all transaction
   * phases.
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult InitMetricInstances() noexcept;

  /**
   * @brief Is called when the transaction execute operation is
   * completed.
   *
   * @param metric_instance The metric instance used to track the execution
   * status.
   * @param http_context The http context of the operation.
   * @param transaction_context The transaction context of the operation.
   */
  virtual void OnTransactionCallback(
      const std::shared_ptr<cpio::AggregateMetricInterface>& metric_instance,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          transaction_context) noexcept;

  /**
   * @brief Executes a transaction phase via the transaction request router.
   *
   * @param metric_instance The metric instance used to track the execution
   * status.
   * @param http_context The http context of the operation.
   * @param transaction_id The id of the transaction.
   * @param transaction_secret The secret of the transaction.
   * @param transaction_origin The origin of the transaction.
   * @param last_execution_timestamp The last execution timestamp.
   * @param transaction_phase The phase of the transaction to be executed.
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult ExecuteTransactionPhase(
      const std::shared_ptr<cpio::AggregateMetricInterface>& metric_instance,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::common::Uuid& transaction_id,
      std::shared_ptr<std::string>& transaction_secret,
      std::shared_ptr<std::string>& transaction_origin,
      core::Timestamp last_execution_timestamp,
      core::TransactionExecutionPhase transaction_phase) noexcept;

  /**
   * @brief Is called when the transaction phase operation is executed.
   *
   * @param metric_instance The metric instance used to track the execution
   * status.
   * @param http_context The http context of the operation.
   * @param transaction_phase_context The context of the transaction phase
   * execution.
   */
  virtual void OnExecuteTransactionPhaseCallback(
      const std::shared_ptr<cpio::AggregateMetricInterface>& metric_instance,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context) noexcept;

  /**
   * @brief Is called once the consume budget transaction has completed.
   *
   * @param consume_budget_transaction The context object of the consume budget
   * transaction.
   * @param transaction_context The context object of the transaction operation.
   */
  virtual void OnExecuteConsumeBudgetTransactionCallback(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context,
      core::AsyncContext<core::TransactionRequest, core::TransactionResponse>&
          transaction_context) noexcept;

  /**
   * @brief Executes the begin transaction phase.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult BeginTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Executes the prepare transaction phase.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult PrepareTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Executes the commit transaction phase.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult CommitTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Executes the notify transaction phase.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult NotifyTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Executes the abort transaction phase.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult AbortTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Executes the end transaction phase.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult EndTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Gets the current transactions status.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Gets the current status of PBS service or its components.
   *
   * Request can contain headers to query a service component's status or the
   * whole PBS service itself.
   *
   * Response body will contain details of the respective component status.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult GetServiceStatus(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Is called when the get transaction status callback is completed.
   *
   * @param metric_instance The metric instance used to track the execution
   * status.
   * @param http_context The http context of the operation.
   * @param get_transaction_status_context The context of the get transaction
   * status operation.
   */
  void OnGetTransactionStatusCallback(
      const std::shared_ptr<cpio::AggregateMetricInterface>& metric_instance,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context,
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept;

  /**
   * @brief Generate one command per budget to consume
   *
   * @param consume_budget_metadata_list
   * @param authorized_domain
   * @param transaction_id
   * @return std::vector<std::shared_ptr<core::TransactionCommand>>
   */
  std::vector<std::shared_ptr<core::TransactionCommand>>
  GenerateConsumeBudgetCommands(
      std::list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
      const std::string& authorized_domain,
      const core::common::Uuid& transaction_id);

  /**
   * @brief Generate several commands each with a batch of budgets to consume
   *
   * @param consume_budget_metadata_list
   * @param authorized_domain
   * @param transaction_id
   * @return std::vector<std::shared_ptr<core::TransactionCommand>>
   */
  std::vector<std::shared_ptr<core::TransactionCommand>>
  GenerateConsumeBudgetCommandsWithBatchesPerDay(
      std::list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
      const std::string& authorized_domain,
      const core::common::Uuid& transaction_id);

  /**
   * @brief Helper to obtain transaction origin from HTTP Request
   *
   * If transaction origin is not supplied in the headers, the authorized domain
   * is used as transaction origin.
   *
   * @param http_context
   * @return std::shared_ptr<std::string>
   */
  std::shared_ptr<std::string> ObtainTransactionOrigin(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>& http_context)
      const;

  /// An instance to the http server.
  std::shared_ptr<core::HttpServerInterface> http_server_;

  /// An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  /// An instance of the transaction request router.
  std::unique_ptr<core::TransactionRequestRouterInterface>
      transaction_request_router_;

  /// @brief An instance of factory to create consume budget commands.
  std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
      consume_budget_command_factory_;

  /// Metric client instance to set up custom metric service.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;

  absl::flat_hash_map<
      std::string,
      absl::flat_hash_map<std::string,
                          std::shared_ptr<cpio::AggregateMetricInterface>>>
      metrics_instances_map_;

  /// An instance of the config provider.
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// The time interval for metrics aggregation.
  core::TimeDuration aggregated_metric_interval_ms_;

  /// Enable generating batch budget consume commands per day
  bool generate_batch_budget_consume_commands_per_day_;

  /// The claimed-identity string of the remote coordinator. This value is
  /// present in the requests coming from the remote coordinator and can be
  /// used to identify such requests.
  std::string remote_coordinator_claimed_identity_;

  bool enable_site_based_authorization_;
};

}  // namespace google::scp::pbs
