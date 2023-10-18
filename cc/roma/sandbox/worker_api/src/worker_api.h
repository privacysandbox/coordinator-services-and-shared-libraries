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
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::roma::sandbox::worker_api {
class WorkerApi : public core::ServiceInterface {
 public:
  struct RunCodeRequest {
    absl::string_view code;
    std::vector<absl::string_view> input;
    absl::flat_hash_map<std::string, std::string> metadata;
    absl::Span<const uint8_t> wasm;
  };

  struct RunCodeResponse {
    std::shared_ptr<std::string> response;
    std::vector<std::shared_ptr<std::string>> errors;
    absl::flat_hash_map<std::string, int64_t> metrics;
  };

  /**
   * @brief Method to execute a code request.
   * @note The implementation of this method must be thread safe.
   */
  virtual core::ExecutionResultOr<RunCodeResponse> RunCode(
      const RunCodeRequest& request) noexcept = 0;

  /**
   * @brief Terminate the underlying worker.
   *
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult Terminate() noexcept = 0;
};
}  // namespace google::scp::roma::sandbox::worker_api
