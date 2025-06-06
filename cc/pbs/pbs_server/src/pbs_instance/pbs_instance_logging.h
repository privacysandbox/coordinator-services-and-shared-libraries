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

#include "cc/core/common/global_logger/src/global_logger.h"

namespace privacy_sandbox::pbs {

static constexpr char kPBSInstance[] = "PBSInstance";

#define INIT_PBS_COMPONENT(component)                                         \
  SCP_INFO(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid,       \
           "PBS component '" #component "' is initializing");                 \
  if (auto execution_result = component->Init();                              \
      !execution_result.Successful()) {                                       \
    SCP_CRITICAL(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid, \
                 execution_result,                                            \
                 "PBS component '" #component "' failed to initialize");      \
    return execution_result;                                                  \
  } else {                                                                    \
    SCP_INFO(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid,     \
             "PBS component '" #component "'successfully initialized");       \
  }

#define RUN_PBS_COMPONENT(component)                                          \
  SCP_INFO(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid,       \
           "PBS component '" #component "' is running");                      \
  if (auto execution_result = component->Run();                               \
      !execution_result.Successful()) {                                       \
    SCP_CRITICAL(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid, \
                 execution_result,                                            \
                 "PBS component '" #component "' failed to run");             \
    return execution_result;                                                  \
  } else {                                                                    \
    SCP_INFO(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid,     \
             "PBS component '" #component "'successfully ran");               \
  }

#define STOP_PBS_COMPONENT(component)                                         \
  SCP_INFO(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid,       \
           "PBS component '" #component "' is stopping");                     \
  if (auto execution_result = component->Stop();                              \
      !execution_result.Successful()) {                                       \
    SCP_CRITICAL(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid, \
                 execution_result,                                            \
                 "PBS component '" #component "' failed to stop");            \
    return execution_result;                                                  \
  } else {                                                                    \
    SCP_INFO(::privacy_sandbox::pbs::kPBSInstance, pbs_common::kZeroUuid,     \
             "PBS component '" #component "'successfully stopped");           \
  }
}  // namespace privacy_sandbox::pbs
