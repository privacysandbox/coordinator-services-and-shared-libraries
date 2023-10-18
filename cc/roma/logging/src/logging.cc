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

#include "logging.h"

#include <limits>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "roma/config/src/config.h"

namespace google::scp::roma::logging {

int GetVlogVerboseLevel() {
  static int external_verbose_level = [] {
    int external_verbose_level = std::numeric_limits<int>::min();
    char* env_var = getenv(kRomaVlogLevel);
    if (!env_var) {
      return external_verbose_level;
    }

    CHECK(absl::SimpleAtoi(env_var, &external_verbose_level) &&
          external_verbose_level >= 0)
        << "ROMA_VLOG_LEVEL needs to be an integer >= 0";

    return external_verbose_level;
  }();
  return external_verbose_level;
}

bool VLogIsOn(int verbose_level) {
  return verbose_level <= GetVlogVerboseLevel();
}
}  // namespace google::scp::roma::logging
