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

#include "public/core/interface/errors.h"

#include <gtest/gtest.h>

#include "public/cpio/interface/error_codes.h"

using google::scp::core::errors::SC_CPIO_INTERNAL_ERROR;
using std::string;

namespace google::scp::core::test {
TEST(ErrorsTest, GetErrorMessageSuccessfully) {
  EXPECT_EQ(string(GetErrorMessage(SC_CPIO_INTERNAL_ERROR)),
            "Internal Error in CPIO");
}
}  // namespace google::scp::core::test
