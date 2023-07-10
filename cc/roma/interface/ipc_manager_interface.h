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
#include "core/interface/service_interface.h"
#include "roma/common/src/role_id.h"

#include "ipc_channel_interface.h"

namespace google::scp::roma {

template <typename TRequest, typename TResponse>
class IpcManagerInterface : public core::ServiceInterface {
 public:
  virtual ~IpcManagerInterface() {}

  /// Set up the default IPC resources to use for current process. This shall be
  /// called in workers first thing after fork.
  virtual core::ExecutionResult SetUpIpcForMyProcess(common::RoleId role) = 0;

  /// Set up the default IPC resources to use for current thread. This shall be
  /// thread-safe as we are going to call in different threads.
  virtual core::ExecutionResult SetUpIpcForMyThread(common::RoleId role) = 0;

  /// Get the IpcChannel for a specific role.
  virtual IpcChannelInterface<TRequest, TResponse>& GetIpcChannel(
      common::RoleId role) = 0;

  /// Get the IpcChannel for current role.
  virtual IpcChannelInterface<TRequest, TResponse>& GetIpcChannel() = 0;
};
}  // namespace google::scp::roma
