// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pbs/budget_key_resolver/src/budget_key_resolver.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <utility>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::pbs::BudgetKeyResolver;
using std::atomic;
using std::make_shared;

namespace google::scp::pbs::test {
TEST(BudgetKeyResolverTest, AlwaysReturnLocal) {
  BudgetKeyResolver budget_key_resolver;
  atomic<bool> condition = false;
  auto request = make_shared<ResolveBudgetKeyRequest>();
  AsyncContext<ResolveBudgetKeyRequest, ResolveBudgetKeyResponse> async_context(
      request, [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->budget_key_location,
                  BudgetKeyLocation::Local);
        condition = true;
      });

  budget_key_resolver.ResolveBudgetKey(async_context);
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::pbs::test
