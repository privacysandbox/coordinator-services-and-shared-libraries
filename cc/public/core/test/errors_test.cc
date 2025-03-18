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

#include "cc/public/core/interface/errors.h"

#include <gtest/gtest.h>

#include <string>

#include "cc/core/interface/errors.h"

namespace google::scp::core::errors {
/// Registers component code as 0x0214 for CPIO common errors.
REGISTER_COMPONENT_CODE(SC_CPIO, 0x0214)

DEFINE_ERROR_CODE(SC_CPIO_INTERNAL_ERROR, SC_CPIO, 0x0001,
                  "Internal Error in CPIO",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors

namespace google::scp::core::test {
TEST(ErrorsTest, GetErrorMessageSuccessfully) {
  EXPECT_EQ(std::string(GetErrorMessage(errors::SC_CPIO_INTERNAL_ERROR)),
            "Internal Error in CPIO");
}
}  // namespace google::scp::core::test
