/*
 * Copyright 2023 Google LLC
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
#include <vector>

#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "roma/sandbox/worker_api/src/worker_api.h"
#include "roma/sandbox/worker_api/src/worker_api_sapi.h"

#include "error_codes.h"
#include "worker_pool.h"

namespace google::scp::roma::sandbox::worker_pool {

class WorkerPoolApiSapi : public WorkerPool {
 public:
  /**
   * @brief Construct a new Worker Pool Api Sapi object
   *
   * @param config The configs for the worker API objects.
   * @param size The size of the pool.
   */
  WorkerPoolApiSapi(const std::vector<worker_api::WorkerApiSapiConfig>& config,
                    size_t size = 4);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  size_t GetPoolSize() noexcept override;

  core::ExecutionResultOr<std::shared_ptr<worker_api::WorkerApi>> GetWorker(
      size_t index) noexcept override;

 protected:
  size_t size_;
  std::vector<std::shared_ptr<worker_api::WorkerApi>> workers_;
};
}  // namespace google::scp::roma::sandbox::worker_pool
