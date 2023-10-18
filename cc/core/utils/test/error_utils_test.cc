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

#include "core/utils/src/error_utils.h"

#include <gtest/gtest.h>

#include "core/interface/errors.h"
#include "core/interface/type_def.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::ResultIs;

namespace google::scp::core::utils::test {
TEST(ErrorUtilsTest, ConvertSuccessExecutionResult) {
  EXPECT_SUCCESS(ConvertToPublicExecutionResult(SuccessExecutionResult()));
}

TEST(ErrorUtilsTest, ConvertFailureExecutionResult) {
  FailureExecutionResult failure(SC_UNKNOWN);
  EXPECT_THAT(ConvertToPublicExecutionResult(failure), ResultIs(failure));
}

TEST(ErrorUtilsTest, ConvertRetryExecutionResult) {
  RetryExecutionResult failure(SC_UNKNOWN);
  EXPECT_THAT(ConvertToPublicExecutionResult(failure), ResultIs(failure));
}
}  // namespace google::scp::core::utils::test
