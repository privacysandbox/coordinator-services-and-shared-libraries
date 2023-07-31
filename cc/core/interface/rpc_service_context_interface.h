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

#include <functional>

namespace google::scp::core {
/**
 * @brief The interface for RPC service contexts. A RPC service context stands
 * for the context of a single RPC request-response invocation. It should have
 * all necessary information for the RPC handling logic.
 */
struct RPCServiceContextInterface {
 public:
  virtual ~RPCServiceContextInterface() = default;

  using RpcHandler = std::function<void(RPCServiceContextInterface&)>;
  /// Prepare for taking requests. User is expected to call this to initiate
  /// reading of a request, or yield until next event.
  virtual ExecutionResult Prepare() noexcept = 0;
  /// Process a request. The Service will call this function to invoke the
  /// designated callback/handler of a request.
  virtual ExecutionResult Process() noexcept = 0;
  /// User is expected to call this after all work are done and proper response
  /// or error is produced.
  virtual ExecutionResult Finish() noexcept = 0;
};
}  // namespace google::scp::core
