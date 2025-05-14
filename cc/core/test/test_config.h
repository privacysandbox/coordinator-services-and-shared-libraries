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

#pragma once

#include <chrono>

namespace privacy_sandbox::pbs_common {
/// Type of duration in milliseconds.
using DurationMs = std::chrono::duration<int64_t, std::milli>;
/// Timeout for all the unit tests in ms.
#define UNIT_TEST_TIME_OUT_MS std::chrono::milliseconds(5000)
/// Short sleep for all the unit tests in ms.
#define UNIT_TEST_SHORT_SLEEP_MS std::chrono::milliseconds(100)
}  // namespace privacy_sandbox::pbs_common
