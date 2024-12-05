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

#include "cc/pbs/interface/budget_key_resolver_interface.h"

namespace google::scp::pbs {
/*! @copydoc BudgetKeyResolverInterface
 */
class BudgetKeyResolver : public BudgetKeyResolverInterface {
 public:
  core::ExecutionResult ResolveBudgetKey(
      core::AsyncContext<ResolveBudgetKeyRequest, ResolveBudgetKeyResponse>&
          resolve_budget_key_context) noexcept override;
};
}  // namespace google::scp::pbs
