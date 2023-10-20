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

#include "public/core/test/interface/struct_matchers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using std::string;
using std::vector;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Gt;
using testing::HasSubstr;
using testing::Pointee;

namespace google::scp::core::test {

struct S {
  int x, y, z;
  string a;
  S* o;
  vector<int> v;
};

TEST(StructMatchersTest, OnePair) {
  S s;
  s.x = 5;
  EXPECT_STRUCT(s, S, x, 5);
  EXPECT_STRUCT(s, S, x, Gt(4));
}

TEST(StructMatchersTest, TwoPair) {
  S s;
  s.x = 5;
  s.y = 10;
  EXPECT_STRUCT(s, S, x, 5, y, 10);
}

S GetS() {
  static int call_count = 0;
  if (call_count != 0)
    ADD_FAILURE() << "GetS called more than once: " << call_count;
  S s;
  s.x = 5;
  s.y = 10;
  call_count++;
  return s;
}

TEST(StructMatchersTest, TwoPairWithFunction) {
  EXPECT_STRUCT(GetS(), S, x, 5, y, 10);
}

TEST(StructMatchersTest, ThreePair) {
  S s;
  s.x = 5;
  s.y = 10;
  s.z = -1;
  EXPECT_STRUCT(s, S, x, 5, y, 10, z, Gt(-2));
}

TEST(StructMatchersTest, FourPair) {
  S s;
  s.x = 5;
  s.y = 10;
  s.z = -1;
  s.a = "foo";
  EXPECT_STRUCT(s, S, x, 5, y, 10, z, Gt(-2), a, "foo");
  EXPECT_STRUCT(s, S, x, 5, y, 10, z, Gt(-2), a, HasSubstr("f"));
}

TEST(StructMatchersTest, FivePair) {
  S s;
  s.x = 5;
  s.y = 10;
  s.z = -1;
  s.a = "foo";
  s.o = &s;
  EXPECT_STRUCT(s, S, x, 5, y, 10, z, Gt(-2), a, "foo", o, &s);
  // We can use a nested matcher as well.
  EXPECT_STRUCT(s, S, x, 5, y, 10, z, Gt(-2), a, "foo", o,
                Pointee(Field(&S::x, 5)));
}

TEST(StructMatchersTest, SixPair) {
  S s;
  s.x = 5;
  s.y = 10;
  s.z = -1;
  s.a = "foo";
  s.o = &s;
  s.v.push_back(s.x);
  s.v.push_back(s.y);
  EXPECT_STRUCT(s, S, x, 5, y, 10, z, Gt(-2), a, "foo", o, &s, v,
                ElementsAre(s.x, s.y));
}

}  // namespace google::scp::core::test
