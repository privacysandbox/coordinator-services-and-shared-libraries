// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CC_PBS_FRONT_END_SERVICE_SRC_FRONT_END_SERVICE_V2
#define CC_PBS_FRONT_END_SERVICE_SRC_FRONT_END_SERVICE_V2

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/provider.h"

namespace google::scp::pbs {

// An implementation of a FrontEndServiceInterface to support relaxed
// consistency in PBS.
class FrontEndServiceV2 : public FrontEndServiceInterface {
 public:
  FrontEndServiceV2(
      std::shared_ptr<core::HttpServerInterface> http_server,
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<core::ConfigProviderInterface> config_provider,
      BudgetConsumptionHelperInterface* budget_consumption_helper,
      core::MetricRouter* metric_router = nullptr);

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;
  core::ExecutionResult ExecuteConsumeBudgetTransaction(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context) noexcept override;

 private:
  // Peer class for testing.
  friend class FrontEndServiceV2Peer;

  // Executes the begin transaction phase.
  core::ExecutionResult BeginTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  // Executes the prepare transaction phase.
  core::ExecutionResult PrepareTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  // Executes the commit transaction phase.
  core::ExecutionResult CommitTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  // Executes the notify transaction phase.
  core::ExecutionResult NotifyTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  // Executes the abort transaction phase.
  core::ExecutionResult AbortTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  // Executes the end transaction phase.
  core::ExecutionResult EndTransaction(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  void OnConsumeBudgetCallback(
      core::AsyncContext<core::HttpRequest, core::HttpResponse> http_context,
      std::string transaction_id,
      core::AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>&
          consume_budget_context);

  // Returns 404 to maintain compatibility with the client code.
  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

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

  // Initializes the metrics.
  void MetricInit() noexcept;

  // An instance to the http server.
  std::shared_ptr<core::HttpServerInterface> http_server_;

  // An instance of the async executor.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;

  // An instance of the config provider.
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  /// The claimed-identity string of the remote coordinator. This value is
  /// present in the requests coming from the remote coordinator and can be
  /// used to identify such requests.
  std::string remote_coordinator_claimed_identity_;

  BudgetConsumptionHelperInterface* budget_consumption_helper_;

  // An instance of metric router which will provide APIs to create metrics.
  core::MetricRouter* metric_router_;

  // OpenTelemetry Meter used for creating and managing metrics.
  std::shared_ptr<opentelemetry::metrics::Meter> meter_;

  // OpenTelemetry instrument for measuring the count of requests.
  // This will be tracked in all the transaction phases.
  std::shared_ptr<opentelemetry::metrics::Counter<uint64_t>>
      total_request_counter_;

  // OpenTelemetry instrument for measuring the count of client errors.
  std::shared_ptr<opentelemetry::metrics::Counter<uint64_t>>
      client_error_counter_;

  // OpenTelemetry instrument for measuring the count of PBS server errors.
  std::shared_ptr<opentelemetry::metrics::Counter<uint64_t>>
      server_error_counter_;

  // OpenTelemetry instrument for measuring the count of keys/budgets per
  // transaction/job.
  std::shared_ptr<opentelemetry::metrics::Histogram<uint64_t>>
      keys_per_transaction_count_;

  // OpenTelemetry instrument for measuring the successful budgets consumed in a
  // transaction.
  std::shared_ptr<opentelemetry::metrics::Histogram<uint64_t>>
      successful_budget_consumed_counter_;

  // OpenTelemetry Instrument for measuring the number of budgets exhausted
  std::shared_ptr<opentelemetry::metrics::Histogram<uint64_t>>
      budgets_exhausted_;
};

}  // namespace google::scp::pbs

#endif  // CC_PBS_FRONT_END_SERVICE_SRC_FRONT_END_SERVICE_V2
