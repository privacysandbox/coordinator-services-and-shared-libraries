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

#include "worker_pool_api_sapi.h"

#include <vector>

#include "absl/log/check.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_WORKER_POOL_WORKER_INDEX_OUT_OF_BOUNDS;
using std::vector;

static constexpr char kConfigAndPoolSizeDoNotMatch[] =
    "ROMA: The worker config vector and the pool size do not match";

namespace google::scp::roma::sandbox::worker_pool {
WorkerPoolApiSapi::WorkerPoolApiSapi(
    const std::vector<worker_api::WorkerApiSapiConfig>& config, size_t size) {
  CHECK(config.size() == size) << kConfigAndPoolSizeDoNotMatch;

  size_ = size;
  for (auto i = 0; i < size_; i++) {
    workers_.push_back(
        std::make_shared<worker_api::WorkerApiSapi>(config.at(i)));
  }
}

ExecutionResult WorkerPoolApiSapi::Init() noexcept {
  for (auto& w : workers_) {
    auto result = w->Init();
    if (!result.Successful()) {
      return result;
    }
  }

  return SuccessExecutionResult();
}

ExecutionResult WorkerPoolApiSapi::Run() noexcept {
  for (auto& w : workers_) {
    auto result = w->Run();
    if (!result.Successful()) {
      return result;
    }
  }

  return SuccessExecutionResult();
}

ExecutionResult WorkerPoolApiSapi::Stop() noexcept {
  for (auto& w : workers_) {
    auto result = w->Stop();
    if (!result.Successful()) {
      return result;
    }
  }

  return SuccessExecutionResult();
}

size_t WorkerPoolApiSapi::GetPoolSize() noexcept {
  return size_;
}

ExecutionResultOr<std::shared_ptr<worker_api::WorkerApi>>
WorkerPoolApiSapi::GetWorker(size_t index) noexcept {
  if (index >= size_) {
    return FailureExecutionResult(
        SC_ROMA_WORKER_POOL_WORKER_INDEX_OUT_OF_BOUNDS);
  }

  return workers_.at(index);
}
}  // namespace google::scp::roma::sandbox::worker_pool
