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

#include "error_codes.h"

namespace google::scp::roma::sandbox::worker {

// Util function for worker.
class WorkerUtils {
 public:
  static core::ExecutionResultOr<std::string> GetValueFromMetadata(
      const absl::flat_hash_map<std::string, std::string>& metadata,
      const std::string& key) noexcept;

  static core::ExecutionResultOr<int> ConvertStrToInt(
      const std::string& value) noexcept;
};
}  // namespace google::scp::roma::sandbox::worker
