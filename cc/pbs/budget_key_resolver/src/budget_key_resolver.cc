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

#include "budget_key_resolver.h"

#include <memory>
#include <utility>

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::make_shared;

namespace google::scp::pbs {
ExecutionResult BudgetKeyResolver::ResolveBudgetKey(
    AsyncContext<ResolveBudgetKeyRequest, ResolveBudgetKeyResponse>&
        resolve_budget_key_context) noexcept {
  resolve_budget_key_context.response = make_shared<ResolveBudgetKeyResponse>();
  resolve_budget_key_context.response->budget_key_location =
      BudgetKeyLocation::Local;
  resolve_budget_key_context.result = SuccessExecutionResult();

  resolve_budget_key_context.Finish();
  return SuccessExecutionResult();
};
}  // namespace google::scp::pbs
