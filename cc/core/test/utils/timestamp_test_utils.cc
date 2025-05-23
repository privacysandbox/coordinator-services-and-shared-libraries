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

#include "timestamp_test_utils.h"

#include <gtest/gtest.h>

#include <google/protobuf/timestamp.pb.h>

using google::protobuf::Timestamp;

namespace privacy_sandbox::pbs_common {
void ExpectTimestampEquals(const Timestamp& target, const Timestamp& expected) {
  EXPECT_EQ(target.seconds(), expected.seconds());
  EXPECT_EQ(target.nanos(), expected.nanos());
}
}  // namespace privacy_sandbox::pbs_common
