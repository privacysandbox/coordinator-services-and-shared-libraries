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

#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "roma/sandbox/worker_api/src/worker_api.h"

namespace google::scp::roma::sandbox::worker_pool {
class WorkerPool : public core::ServiceInterface {
 public:
  /**
   * @brief Get the Pool Size
   *
   * @return size_t
   */
  virtual size_t GetPoolSize() noexcept = 0;

  /**
   * @brief Get a worker by index. Will return failure if bad index.
   *
   * @param index
   * @return core::ExecutionResultOr<std::shared_ptr<worker_api::WorkerApi>>
   */
  virtual core::ExecutionResultOr<std::shared_ptr<worker_api::WorkerApi>>
  GetWorker(size_t index) noexcept = 0;
};
}  // namespace google::scp::roma::sandbox::worker_pool
