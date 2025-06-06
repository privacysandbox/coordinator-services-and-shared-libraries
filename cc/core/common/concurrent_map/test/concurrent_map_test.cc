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

#include "cc/core/common/concurrent_map/src/concurrent_map.h"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/test/scp_test_base.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {

class ConcurrentMapTests : public ScpTestBase {};

TEST_F(ConcurrentMapTests, InsertElement) {
  ConcurrentMap<int, int> map;

  int i;
  auto result = map.Insert(std::make_pair(1, 1), i);

  EXPECT_SUCCESS(result);
  EXPECT_EQ(i, 1);
}

TEST_F(ConcurrentMapTests, InsertExistingElement) {
  ConcurrentMap<int, int> map;

  int i;
  auto result = map.Insert(std::make_pair(1, 1), i);
  result = map.Insert(std::make_pair(1, 1), i);

  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));
}

TEST_F(ConcurrentMapTests, DeleteExistingElement) {
  ConcurrentMap<int, int> map;

  int val = 1, key = 2;
  auto result = map.Insert(std::make_pair(key, val), val);
  result = map.Erase(key);

  EXPECT_SUCCESS(result);

  result = map.Find(key, val);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
}

TEST_F(ConcurrentMapTests, DeleteNonExistingElement) {
  ConcurrentMap<int, int> map;
  int i = 0;
  auto result = map.Erase(i);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
}

TEST_F(ConcurrentMapTests, FindAnExistingElement) {
  ConcurrentMap<int, int> map;

  int i;
  int value;
  auto result = map.Insert(std::make_pair(1, 1), i);
  result = map.Find(i, value);

  EXPECT_SUCCESS(result);
  EXPECT_EQ(value, 1);
}

TEST_F(ConcurrentMapTests, FindAnExistingElementUuid) {
  ConcurrentMap<Uuid, Uuid, UuidCompare> map;

  Uuid uuid_key = Uuid::GenerateUuid();
  Uuid uuid_value = Uuid::GenerateUuid();

  Uuid value;
  auto result = map.Insert(std::make_pair(uuid_key, uuid_value), uuid_value);
  result = map.Find(uuid_key, value);

  EXPECT_SUCCESS(result);
  EXPECT_EQ(value, uuid_value);
}

TEST_F(ConcurrentMapTests, GetKeys) {
  ConcurrentMap<Uuid, Uuid, UuidCompare> map;

  Uuid uuid_key = Uuid::GenerateUuid();
  Uuid uuid_value = Uuid::GenerateUuid();

  Uuid uuid_key1 = Uuid::GenerateUuid();
  Uuid uuid_value1 = Uuid::GenerateUuid();

  auto result = map.Insert(std::make_pair(uuid_key, uuid_value), uuid_value);
  EXPECT_SUCCESS(result);

  result = map.Insert(std::make_pair(uuid_key1, uuid_value1), uuid_value1);
  EXPECT_SUCCESS(result);

  std::vector<Uuid> keys;
  result = map.Keys(keys);
  EXPECT_SUCCESS(result);

  if (keys[0] == uuid_key) {
    EXPECT_EQ(keys[1], uuid_key1);
  } else if (keys[0] == uuid_key1) {
    EXPECT_EQ(keys[1], uuid_key);
  } else {
    EXPECT_EQ(true, false);
  }
}
}  // namespace privacy_sandbox::pbs_common
