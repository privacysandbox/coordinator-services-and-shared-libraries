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
#include <memory>

#include "core/interface/service_interface.h"

namespace google::scp::roma {
/**
 * IpcChannelInterface defines basic IPC interfaces required for communication
 * between dispatcher and workers. It should work like a bi-directional socket
 * that both ends can read and write.
 */
template <typename TRequest, typename TResponse>
class IpcChannelInterface : public core::ServiceInterface {
 public:
  /// Try to acquire a slot in the IPC channel to push a request.
  virtual core::ExecutionResult TryAcquirePushRequest() = 0;

  /// Push a request from dispatcher to worker.
  virtual core::ExecutionResult PushRequest(
      std::unique_ptr<TRequest> request) = 0;
  /// Pop a request (typically on worker).
  virtual core::ExecutionResult PopRequest(TRequest*& request) = 0;

  /// Push a response from worker to dispatcher.
  virtual core::ExecutionResult PushResponse(
      std::unique_ptr<TResponse> response) = 0;
  /// Pop a response on dispatcher side.
  virtual core::ExecutionResult PopResponse(
      std::unique_ptr<TResponse>& response) = 0;

  // Virtual destructor to avoid memleaks.
  virtual ~IpcChannelInterface() {}
};  // class IpcChannelInterface

}  // namespace google::scp::roma
