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

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/pbs/consume_budget/src/budget_consumer.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "opentelemetry/metrics/meter.h"

namespace privacy_sandbox::pbs {

// An implementation of a FrontEndServiceInterface to support relaxed
// consistency in PBS.
class FrontEndServiceV2 : public FrontEndServiceInterface {
 public:
  FrontEndServiceV2(
      std::shared_ptr<pbs_common::HttpServerInterface> http_server,
      std::shared_ptr<pbs_common::AsyncExecutorInterface> async_executor,
      std::shared_ptr<pbs_common::ConfigProviderInterface> config_provider,
      BudgetConsumptionHelperInterface* budget_consumption_helper,
      pbs_common::MetricRouter* metric_router = nullptr);

  pbs_common::ExecutionResult Init() noexcept override;
  pbs_common::ExecutionResult Run() noexcept override;
  pbs_common::ExecutionResult Stop() noexcept override;

 private:
  // Peer class for testing.
  friend class FrontEndServiceV2Peer;

  // Executes common transaction phase
  pbs_common::ExecutionResult CommonTransactionProcess(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context,
      absl::string_view transaction_phase);

  // Executes the begin transaction phase.
  pbs_common::ExecutionResult BeginTransaction(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context);

  // Executes the prepare transaction phase.
  pbs_common::ExecutionResult PrepareTransaction(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context);

  // Executes the commit transaction phase.
  pbs_common::ExecutionResult CommitTransaction(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context);

  // Executes the notify transaction phase.
  pbs_common::ExecutionResult NotifyTransaction(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context);

  // Executes the abort transaction phase.
  pbs_common::ExecutionResult AbortTransaction(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context);

  // Executes the end transaction phase.
  pbs_common::ExecutionResult EndTransaction(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context);

  void OnConsumeBudgetCallback(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>
          http_context,
      std::string transaction_id,
      pbs_common::AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>&
          consume_budget_context);

  // Returns 404 to maintain compatibility with the client code.
  pbs_common::ExecutionResult GetTransactionStatus(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context);

  // Returns the BudgetConsumer interface based on first seen "budget_type"
  pbs_common::ExecutionResultOr<std::unique_ptr<BudgetConsumer>>
  GetBudgetConsumer(const privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest&
                        request_proto);

  pbs_common::ExecutionResult ParseRequestWithBudgetConsumer(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context,
      absl::string_view transaction_id,
      ConsumeBudgetsRequest& consume_budget_request);

  /**
   * @brief Helper to obtain transaction origin from HTTP Request
   *
   * If transaction origin is not supplied in the headers, the authorized domain
   * is used as transaction origin.
   *
   * @param http_context
   * @return std::shared_ptr<std::string>
   */
  std::string ObtainTransactionOrigin(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>& http_context) const;

  // Initializes the metrics.
  void MetricInit();

  // An instance to the http server.
  std::shared_ptr<pbs_common::HttpServerInterface> http_server_;

  // An instance of the async executor.
  std::shared_ptr<pbs_common::AsyncExecutorInterface> async_executor_;

  // An instance of the config provider.
  std::shared_ptr<pbs_common::ConfigProviderInterface> config_provider_;

  /// The claimed-identity string of the remote coordinator. This value is
  /// present in the requests coming from the remote coordinator and can be
  /// used to identify such requests.
  std::string remote_coordinator_claimed_identity_;

  BudgetConsumptionHelperInterface* budget_consumption_helper_;

  // An instance of metric router which will provide APIs to create metrics.
  pbs_common::MetricRouter* metric_router_;

  // OpenTelemetry Meter used for creating and managing metrics.
  std::shared_ptr<opentelemetry::metrics::Meter> meter_;

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

}  // namespace privacy_sandbox::pbs

#endif  // CC_PBS_FRONT_END_SERVICE_SRC_FRONT_END_SERVICE_V2
