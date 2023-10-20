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

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

namespace google::scp::core::test {

// Convenience macro for shortening this pattern:
//
// Struct S { int x; string y; };
// TEST(...) {
//   S s;
//   ...
//   EXPECT_EQ(s.x, 5);
//   EXPECT_EQ(s.y, "foo");
// }
//
// // Instead it becomes:
// TEST(...) {
//   S s;
//   ...
//   EXPECT_STRUCT(s, S, x, 5, y, "foo");
// }
//
// This should be called with the variable name, the variable's type, followed
// by a # of pairs of arguments. These will be 1) the name of the field we will
// be matching and 2) the matcher for this field. Note that for the matcher it
// can simply be a value if you wish to use the operator== method or you can use
// another matcher like Pointee(), ElementsAre(), or HasSubstr() for example.
#define EXPECT_STRUCT(lhs, class_name, ...)                            \
  __EXPECT_STRUCT_HELPER(lhs, __UNIQUE_VAR_NAME(__LINE__), class_name, \
                         __VA_ARGS__)

// This macro uses the # of variables in __VA_ARGS__ to determine which macro
// overload is used.
// __EXPECT_STRUCT_CHOOSER is designed to accept 13 arguments. Any further
// arguments are discarded.
// __EXPECT_STRUCT_CHOOSER always emits the final named argument called NAME.
// All of the arguments to the chooser are *names* of all the macro overloads
// except for the ones corresponding to odd # arguments.
// e.g. __VA_ARGS__ has 2 arguments (let's call them x1, y1)
//
// We get: __EXPECT_STRUCT_CHOOSER(x1, y1, __EXPECT_STRUCT6, __BAD_RESOLUTION,
// __EXPECT_STRUCT5, __BAD_RESOLUTION, __EXPECT_STRUCT4, __BAD_RESOLUTION,
// __EXPECT_STRUCT3, __BAD_RESOLUTION, __EXPECT_STRUCT2, __BAD_RESOLUTION,
// __EXPECT_STRUCT1, __BAD_RESOLUTION)
//  ^^^^ NAME ^^^^     ^^^^ ... ^^^^
//
// So __EXPECT_STRUCT1 is chosen and then called with the arguments (temp_name,
// class_name, __VA_ARGS__).
//
// Essentially the # of __VA_ARGS__ chooses which macro name to call by pushing
// the proper element into the NAME argument position.
//
// We make a reference to lhs in case it is a function so we avoid repeated
// calls.
#define __EXPECT_STRUCT_HELPER(lhs, temp_name, class_name, ...)               \
  auto&& temp_name = lhs;                                                     \
  __EXPECT_STRUCT_CHOOSER(                                                    \
      __VA_ARGS__, __EXPECT_STRUCT6, __BAD_RESOLUTION, __EXPECT_STRUCT5,      \
      __BAD_RESOLUTION, __EXPECT_STRUCT4, __BAD_RESOLUTION, __EXPECT_STRUCT3, \
      __BAD_RESOLUTION, __EXPECT_STRUCT2, __BAD_RESOLUTION, __EXPECT_STRUCT1, \
      __BAD_RESOLUTION)                                                       \
  (temp_name, class_name, __VA_ARGS__)

#define __BAD_RESOLUTION(...)                                               \
  static_assert(false,                                                      \
                "EXPECT_STRUCT should be called with an even # of args at " \
                "least 1 pair but no "                                      \
                "more than 6 total pairs")

#define __UNIQUE_VAR_NAME_HELPER(x, y) x##y
#define __UNIQUE_VAR_NAME(x) __UNIQUE_VAR_NAME_HELPER(__var, x)

// Call with the *names* of all of the macros to use for each # of arguments.
// The macros accepting a higher number of arguments should appear earlier in
// the argument list followed by those accepting fewer number of arguments.
// Exactly 12 macro names should be passed preceded by the actual variadic
// arguments to choose the proper macro overload. The ellipsis (...) is so that
// the macro can catch any argument that is pushed out of the named arguments.
#define __EXPECT_STRUCT_CHOOSER(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, \
                                _12, NAME, ...)                               \
  NAME

#define __MATCH_FIELD(lhs, class_name, field, matcher) \
  EXPECT_THAT(lhs, ::testing::Field(#field, &class_name::field, matcher))

#define __EXPECT_STRUCT1(lhs, class_name, field1, matcher1) \
  __MATCH_FIELD(lhs, class_name, field1, matcher1)

#define __EXPECT_STRUCT2(lhs, class_name, field1, matcher1, field2, matcher2) \
  __EXPECT_STRUCT1(lhs, class_name, field1, matcher1);                        \
  __EXPECT_STRUCT1(lhs, class_name, field2, matcher2)

#define __EXPECT_STRUCT3(lhs, class_name, field1, matcher1, field2, matcher2, \
                         field3, matcher3)                                    \
  __EXPECT_STRUCT2(lhs, class_name, field1, matcher1, field2, matcher2);      \
  __EXPECT_STRUCT1(lhs, class_name, field3, matcher3)

#define __EXPECT_STRUCT4(lhs, class_name, field1, matcher1, field2, matcher2, \
                         field3, matcher3, field4, matcher4)                  \
  __EXPECT_STRUCT3(lhs, class_name, field1, matcher1, field2, matcher2,       \
                   field3, matcher3);                                         \
  __EXPECT_STRUCT1(lhs, class_name, field4, matcher4)

#define __EXPECT_STRUCT5(lhs, class_name, field1, matcher1, field2, matcher2,  \
                         field3, matcher3, field4, matcher4, field5, matcher5) \
  __EXPECT_STRUCT4(lhs, class_name, field1, matcher1, field2, matcher2,        \
                   field3, matcher3, field4, matcher4);                        \
  __EXPECT_STRUCT1(lhs, class_name, field5, matcher5)

#define __EXPECT_STRUCT6(lhs, class_name, field1, matcher1, field2, matcher2,  \
                         field3, matcher3, field4, matcher4, field5, matcher5, \
                         field6, matcher6)                                     \
  __EXPECT_STRUCT5(lhs, class_name, field1, matcher1, field2, matcher2,        \
                   field3, matcher3, field4, matcher4, field5, matcher5);      \
  __EXPECT_STRUCT1(lhs, class_name, field6, matcher6)

}  // namespace google::scp::core::test
