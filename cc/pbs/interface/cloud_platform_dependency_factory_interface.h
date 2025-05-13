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

#include "cc/core/interface/authorization_proxy_interface.h"
#include "cc/core/interface/http_client_interface.h"
#include "cc/core/interface/initializable_interface.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/pbs/interface/consume_budget_interface.h"

namespace privacy_sandbox::pbs {

/// @brief Callbacks originating from the providers should have a higher
/// priority than the regular tasks because they are time-sensitive.
static constexpr pbs_common::AsyncPriority
    kDefaultAsyncPriorityForCallbackExecution = pbs_common::AsyncPriority::High;

/// @brief Blocking tasks are scheduled with a normal priority are can be
/// starved by a higher/urgent priority tasks.
static constexpr pbs_common::AsyncPriority
    kDefaultAsyncPriorityForBlockingIOTaskExecution =
        pbs_common::AsyncPriority::Normal;

/**
 * @brief Platform specific factory interface to provide platform specific
 * clients to PBS
 *
 */
class CloudPlatformDependencyFactoryInterface
    : public pbs_common::InitializableInterface {
 public:
  virtual ~CloudPlatformDependencyFactoryInterface() = default;

  /**
   * @brief Construct a client for PBS to talk to authentication
   * endpoint
   *
   * @param async_executor
   * @param http_client
   * @return std::unique_ptr<AuthorizationProxyInterface>
   */
  virtual std::unique_ptr<pbs_common::AuthorizationProxyInterface>
  ConstructAuthorizationProxyClient(
      std::shared_ptr<pbs_common::AsyncExecutorInterface> async_executor,
      std::shared_ptr<pbs_common::HttpClientInterface>
          http_client) noexcept = 0;

  // Constructs an AWS Client to talk to the authentication endpoint. This is
  // only used on GCP to authenticate requests that come from AWS PBS to GCP PBS
  // via DNS.
  virtual std::unique_ptr<pbs_common::AuthorizationProxyInterface>
  ConstructAwsAuthorizationProxyClient(
      std::shared_ptr<pbs_common::AsyncExecutorInterface> async_executor,
      std::shared_ptr<pbs_common::HttpClientInterface>
          http_client) noexcept = 0;

  virtual std::unique_ptr<pbs::BudgetConsumptionHelperInterface>
  ConstructBudgetConsumptionHelper(
      pbs_common::AsyncExecutorInterface* async_executor,
      pbs_common::AsyncExecutorInterface* io_async_executor) noexcept = 0;

  /**
   * @brief Construct Metric Router for Otel metrics collection
   * @param Optional instance_client_provider
   * @return std::unique_ptr<pbs_common::MetricRouter>
   */
  virtual std::unique_ptr<pbs_common::MetricRouter>
  ConstructMetricRouter() noexcept = 0;
};

}  // namespace privacy_sandbox::pbs
