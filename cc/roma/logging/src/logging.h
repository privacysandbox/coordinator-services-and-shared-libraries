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

#pragma once

#include "absl/log/log.h"

#ifndef ROMA_VLOG

#define ROMA_VLOG_IS_ON(verbose_level) \
  ::google::scp::roma::logging::VLogIsOn(verbose_level)

#define ROMA_VLOG(verbose_level)                            \
  for (int roma_logging_verbose_level = (verbose_level),    \
           roma_logging_log_loop = 1;                       \
       roma_logging_log_loop; roma_logging_log_loop = 0)    \
  LOG_IF(INFO, ROMA_VLOG_IS_ON(roma_logging_verbose_level)) \
      .WithVerbosity(roma_logging_verbose_level)

namespace google::scp::roma::logging {
// Check whether the verbose_level is smaller than ROMA_VLOG_LEVEL.
bool VLogIsOn(int verbose_level);

// Read the ROMA VLOG verbose level from environment variable.
int GetVlogVerboseLevel();
}  // namespace google::scp::roma::logging

#endif
