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

#include <atomic>
#include <string>

#include "core/interface/traffic_forwarder_interface.h"

namespace google::scp::core {
class TCPTrafficForwarderSocat : public core::TrafficForwarderInterface {
 public:
  TCPTrafficForwarderSocat(const std::string& local_port,
                           const std::string& forwarding_address = "");

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult ResetForwardingAddress(
      const std::string& forwarding_address) noexcept override;

 private:
  std::string local_port_;
  std::string forwarding_address_;
  std::atomic<int> child_pid_ = -1;
};
}  // namespace google::scp::core
