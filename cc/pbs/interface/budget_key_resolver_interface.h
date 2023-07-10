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
#include "core/interface/service_interface.h"
#include "pbs/budget_key/src/budget_key.h"

#include "type_def.h"

namespace google::scp::pbs {

/// TODO: This structure must be updated when multi-instance PBS is supported.
enum class BudgetKeyLocation { Local = 1, Remote = 2, Unknown = 3 };

/// Is used to resolve a specfic budget key.
struct ResolveBudgetKeyRequest {
  /// The name of the budget key to resolve.
  std::shared_ptr<BudgetKey> budget_key_name;
};

/**
 * @brief The response of resolve budget key request with the location of the
 * budget key.
 */
struct ResolveBudgetKeyResponse {
  /// The current location of the key.
  BudgetKeyLocation budget_key_location = BudgetKeyLocation::Unknown;
};

/**
 * @brief Responsible to resolve the budget key and return the related location
 * info.
 */
class BudgetKeyResolverInterface {
 public:
  virtual ~BudgetKeyResolverInterface() = default;

  /**
   * @brief Resolves a specific budget key location.
   *
   * @param resolve_budget_key_context the async context of the budget key
   * resolution operation.
   * @return core::ExecutionResult the result of the operation.
   */
  virtual core::ExecutionResult ResolveBudgetKey(
      core::AsyncContext<ResolveBudgetKeyRequest, ResolveBudgetKeyResponse>&
          resolve_budget_key_context) noexcept = 0;
};
}  // namespace google::scp::pbs
