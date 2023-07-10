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
#include <memory>
#include <string>

#include "async_context.h"
#include "message_router_interface.h"
#include "rpc_service_context_interface.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {

class NetworkServiceInterface : public ServiceInterface {
 public:
  virtual ~NetworkServiceInterface() = default;

  enum class AddressType {
    kDNS,    /// Dns-based address, e.g. example.com:5678
    kUNIX,   /// Unix domain sockets, e.g. /path/to/uds
    kIPV4,   /// IPv4 addresses, e.g. 127.0.0.1:5678
    kIPV6,   /// IPv6 addresses, e.g. [DEAD:BEEF:ACED::1]:5678
    kVSOCK,  /// VSock addresses, e.g. "3:5678"
  };
  using Address = std::string;
  using Uri = std::string;

  /// Listen on a specified address.
  virtual ExecutionResult Listen(const Address& addr) = 0;

  /**
   * @brief Register a handler of a RPC uri.
   *
   * @param uri The uri of the RPC method.
   * @param handler The handler to call when a RPC request is ready.
   * @return ExecutionResult
   */
  virtual ExecutionResult RegisterHandler(
      const Uri& uri,
      const RPCServiceContextInterface::RpcHandler& handler) = 0;
};
}  // namespace google::scp::core
