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

#include "core/common/lru_cache/src/lru_cache.h"

#include <gtest/gtest.h>

#include <string>

using std::string;
using std::to_string;

namespace google::scp::core::common::test {
TEST(LruCacheTest, CanAddAndGetElement) {
  LruCache<string, string> cache(10);
  auto key = "Some Key";
  auto value = "Some value";

  cache.Set(key, value);

  EXPECT_TRUE(cache.Contains(key));

  auto read_value = cache.Get(key);

  EXPECT_EQ(read_value, value);
}

TEST(LruCacheTest, ClearShouldEmptyCache) {
  LruCache<string, string> cache(10);
  auto key = "Some Key";
  auto value = "Some value";

  cache.Set(key, value);
  EXPECT_TRUE(cache.Contains(key));

  EXPECT_EQ(cache.Size(), 1);

  cache.Clear();

  EXPECT_EQ(cache.Size(), 0);
}

TEST(LruCacheTest, CanAddAndGetMultipleElements) {
  LruCache<string, string> cache(10);

  for (int i = 0; i < 10; i++) {
    auto key = "Some Key" + to_string(i);
    auto value = "Some value" + to_string(i);

    cache.Set(key, value);

    EXPECT_TRUE(cache.Contains(key));

    auto read_value = cache.Get(key);

    EXPECT_EQ(read_value, value);
  }
}

TEST(LruCacheTest, ShouldReplaceOldestItem) {
  LruCache<string, string> cache(5);

  // We add 5 items
  for (int i = 0; i < 5; i++) {
    auto key = "Some Key" + to_string(i);
    auto value = "Some value" + to_string(i);

    cache.Set(key, value);
    EXPECT_TRUE(cache.Contains(key));

    auto read_value = cache.Get(key);
    EXPECT_EQ(read_value, value);
  }

  EXPECT_EQ(cache.Size(), 5);

  // We add a new element, and since the cache is at capacity, we should replace
  // the oldest element (the first one that was added)
  auto new_key = "New key";
  auto new_value = "New Value";
  cache.Set(new_key, new_value);
  EXPECT_TRUE(cache.Contains(new_key));

  auto read_value = cache.Get(new_key);
  EXPECT_EQ(read_value, new_value);

  // Size should still be 5
  EXPECT_EQ(cache.Size(), 5);

  // The first element that was inserted shouldn't exist
  EXPECT_FALSE(cache.Contains("Some Key0"));
  // The rest should exist
  for (int i = 1; i < 5; i++) {
    auto key = "Some Key" + to_string(i);
    auto value = "Some value" + to_string(i);

    EXPECT_TRUE(cache.Contains(key));
    auto read_value = cache.Get(key);
    EXPECT_EQ(read_value, value);
  }
}

TEST(LruCacheTest, LruPolicyShouldBeAffectedByGets) {
  LruCache<string, string> cache(2);

  auto key1 = "Key1";
  auto value1 = "Value1";
  cache.Set(key1, value1);

  auto key2 = "Key2";
  auto value2 = "Value2";
  cache.Set(key2, value2);

  // If we were to add an element as it is, then key1 should be evicted.
  // However, if we touch key1, then key2 should be evicted. Touch key1.
  EXPECT_EQ(cache.Get(key1), value1);

  auto key3 = "Key3";
  auto value3 = "Value3";

  cache.Set(key3, value3);

  // Now key2 shouldn't be there
  EXPECT_FALSE(cache.Contains(key2));

  EXPECT_TRUE(cache.Contains(key1));
  EXPECT_TRUE(cache.Contains(key3));
}

TEST(LruCacheTest, LruPolicyShouldBeAffectedBySets) {
  LruCache<string, string> cache(2);

  auto key1 = "Key1";
  auto value1 = "Value1";
  cache.Set(key1, value1);

  auto key2 = "Key2";
  auto value2 = "Value2";
  cache.Set(key2, value2);

  // If we were to add an element as it is, then key1 should be evicted.
  // However, if we touch key1, then key2 should be evicted. Touch key1.
  auto new_value1 = "NewValue1";
  cache.Set(key1, new_value1);

  auto key3 = "Key3";
  auto value3 = "Value3";

  cache.Set(key3, value3);

  // Now key2 shouldn't be there
  EXPECT_FALSE(cache.Contains(key2));

  EXPECT_TRUE(cache.Contains(key1));
  EXPECT_TRUE(cache.Contains(key3));
}

TEST(LruCacheTest, ShouldBeAbleToReplaceValues) {
  LruCache<string, string> cache(2);

  auto key1 = "Key1";
  auto value1 = "Value1";
  cache.Set(key1, value1);
  EXPECT_EQ(cache.Get(key1), value1);

  auto key2 = "Key2";
  auto value2 = "Value2";
  cache.Set(key2, value2);

  auto new_value1 = "NewValue1";
  cache.Set(key1, new_value1);

  EXPECT_EQ(cache.Get(key1), new_value1);
}

TEST(LruCacheTest, ShouldBeAbleToGetAllItems) {
  LruCache<string, string> cache(2);

  auto key1 = "Key1";
  auto value1 = "Value1";
  cache.Set(key1, value1);
  EXPECT_EQ(cache.Get(key1), value1);

  auto key2 = "Key2";
  auto value2 = "Value2";
  cache.Set(key2, value2);

  auto all_items = cache.GetAll();

  EXPECT_EQ(2, all_items.size());
  EXPECT_EQ(all_items[key1], value1);
  EXPECT_EQ(all_items[key2], value2);
}
}  // namespace google::scp::core::common::test
