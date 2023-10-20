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
#include <string>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

namespace google::scp::cpio::client_providers {
/// Configurations for AutoScalingClient.
struct AutoScalingClientOptions {
  virtual ~AutoScalingClientOptions() = default;
};

/**
 * @brief Responsible to handle auto scalings.
 */
class AutoScalingClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~AutoScalingClientProviderInterface() = default;

  /**
   * @brief If the given instance is in TERMINATING_WAIT state, schedule the
   * termination immediately. If the given instance is not in TERMINATING_WAIT
   * state, do nothing.
   *
   * @param try_finish_termination_context the context of the operation.
   * @return core::ExecutionResult the execution result of the operation.
   */
  virtual core::ExecutionResult TryFinishInstanceTermination(
      core::AsyncContext<cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationRequest,
                         cmrt::sdk::auto_scaling_service::v1::
                             TryFinishInstanceTerminationResponse>&
          try_finish_termination_context) noexcept = 0;
};

class AutoScalingClientProviderFactory {
 public:
  /**
   * @brief Factory to create AutoScalingClientProvider.
   *
   * @return std::shared_ptr<AutoScalingClientProviderInterface> created
   * AutoScalingClientProvider.
   */
  static std::shared_ptr<AutoScalingClientProviderInterface> Create(
      const std::shared_ptr<AutoScalingClientOptions>& options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor);
};
}  // namespace google::scp::cpio::client_providers
