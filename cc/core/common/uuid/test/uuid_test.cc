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

#include "cc/core/common/uuid/src/uuid.h"

#include <gtest/gtest.h>

#include <string>

#include "cc/core/common/uuid/src/error_codes.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {
TEST(UuidTests, UuidGeneration) {
  Uuid uuid = Uuid::GenerateUuid();

  EXPECT_NE(uuid.high, 0);
  EXPECT_NE(uuid.low, 0);
}

TEST(UuidTests, UuidToString) {
  Uuid uuid = Uuid::GenerateUuid();

  auto uuid_string = ToString(uuid);
  Uuid parsed_uuid;
  EXPECT_SUCCESS(FromString(uuid_string, parsed_uuid));
  EXPECT_EQ(parsed_uuid, uuid);
}

TEST(UuidTests, InvalidUuidString) {
  std::string uuid_string = "123";
  Uuid parsed_uuid;
  EXPECT_THAT(FromString(uuid_string, parsed_uuid),
              ResultIs(FailureExecutionResult(SC_UUID_INVALID_STRING)));

  uuid_string = "3E2A3D09r48EDrA355rD346rAD7DC6CB0909";
  EXPECT_THAT(FromString(uuid_string, parsed_uuid),
              ResultIs(FailureExecutionResult(SC_UUID_INVALID_STRING)));

  uuid_string = "3E2A3D09-48RD-A355-D346-AD7DC6CB0909";
  EXPECT_THAT(FromString(uuid_string, parsed_uuid),
              ResultIs(FailureExecutionResult(SC_UUID_INVALID_STRING)));

  uuid_string = "3E2A3D09-48Ed-A355-D346-AD7DC6CB0909";
  EXPECT_THAT(FromString(uuid_string, parsed_uuid),
              ResultIs(FailureExecutionResult(SC_UUID_INVALID_STRING)));
}
}  // namespace privacy_sandbox::pbs_common
