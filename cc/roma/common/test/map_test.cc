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

#include <gtest/gtest.h>

#include <string>

#include "roma/common/src/containers.h"

using std::string;
using std::to_string;

namespace google::scp::roma::common::test {
TEST(MapTest, KeyShouldBeInOrderOfInsertion) {
  Map<string, string> map;
  for (int i = 0; i < 30; i++) {
    map.Set(to_string(i), to_string(i));
  }

  auto keys = map.Keys();
  EXPECT_EQ(30, keys.size());

  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(keys.at(i), to_string(i));
  }
}

TEST(MapTest, RemovingKeysShouldRespectOrder) {
  Map<string, string> map;
  for (int i = 0; i < 30; i++) {
    map.Set(to_string(i), to_string(i));
  }

  map.Delete("5");
  map.Delete("10");

  auto keys = map.Keys();
  // We removed 2 elements
  EXPECT_EQ(28, keys.size());

  EXPECT_FALSE(map.Contains("5"));
  EXPECT_FALSE(map.Contains("10"));

  auto keys_it = keys.begin();

  for (int i = 0; i < 30; i++) {
    if (i == 5 || i == 10) {
      continue;
    }
    EXPECT_EQ(*keys_it, to_string(i));
    keys_it++;
  }
}

TEST(MapTest, InlineKeysAndValues) {
  Map<string, string> map;
  map.Set("key", "val");
  EXPECT_EQ(map.Get("key"), "val");
  EXPECT_TRUE(map.Contains("key"));
  map.Delete("key");
  EXPECT_EQ(map.Size(), 0);
  EXPECT_EQ(map.Keys().size(), 0);
}

TEST(MapTest, ReferenceKeysAndValues) {
  string key = "key";
  string val = "val";
  Map<string, string> map;
  map.Set(key, val);
  EXPECT_EQ(map.Get(key), val);
  EXPECT_TRUE(map.Contains(key));
  map.Delete(key);
  EXPECT_EQ(map.Size(), 0);
  EXPECT_EQ(map.Keys().size(), 0);
}

TEST(MapTest, SameKeyShouldReplace) {
  Map<string, string> map;
  map.Set("key", "val");
  EXPECT_EQ(map.Get("key"), "val");

  map.Set("key", "new_val");
  EXPECT_EQ(map.Get("key"), "new_val");
}

TEST(MapTest, SameKeyShouldReplaceUsingRef) {
  Map<string, string> map;
  auto key = "key";
  auto val = "val";
  auto new_val = "new_val";

  map.Set(key, val);
  EXPECT_EQ(map.Get(key), val);

  map.Set(key, new_val);
  EXPECT_EQ(map.Get(key), new_val);
}

TEST(MapTest, ReplacingValueShouldNotChangeInsertionOrder) {
  Map<string, string> map;
  map.Set("key1", "val1");
  map.Set("key2", "val2");
  map.Set("key3", "val3");

  // Replace value
  map.Set("key1", "new_val1");

  auto keys = map.Keys();

  EXPECT_EQ(keys.size(), 3);
  EXPECT_EQ(keys.at(0), "key1");
  EXPECT_EQ(keys.at(1), "key2");
  EXPECT_EQ(keys.at(2), "key3");
}

TEST(MapTest, ReplacingValueUsingRefShouldNotChangeInsertionOrder) {
  Map<string, string> map;
  auto key1 = "key1";
  auto val1 = "val1";
  auto key2 = "key2";
  auto val2 = "val2";
  auto key3 = "key3";
  auto val3 = "val3";

  auto new_val = "new_val1";

  map.Set(key1, val1);
  map.Set(key2, val2);
  map.Set(key3, val3);

  // Replace value
  map.Set(key1, new_val);
  EXPECT_EQ(map.Get(key1), new_val);

  auto keys = map.Keys();

  EXPECT_EQ(keys.size(), 3);
  EXPECT_EQ(keys.at(0), "key1");
  EXPECT_EQ(keys.at(1), "key2");
  EXPECT_EQ(keys.at(2), "key3");
}

TEST(MapTest, ClearShouldCleanData) {
  Map<string, string> map;
  map.Set("key", "val");
  map.Set("key2", "val2");

  EXPECT_EQ(map.Size(), 2);
  EXPECT_EQ(map.Keys().size(), 2);

  map.Clear();

  EXPECT_EQ(map.Size(), 0);
  EXPECT_EQ(map.Keys().size(), 0);
}
}  // namespace google::scp::roma::common::test
