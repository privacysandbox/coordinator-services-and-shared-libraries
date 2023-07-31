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

#include <string>

#include "core/interface/traffic_forwarder_interface.h"

namespace google::scp::core::tcp_traffic_forwarder::mock {
class MockTCPTrafficForwarder : public core::TrafficForwarderInterface {
 public:
  MockTCPTrafficForwarder() = default;

  ExecutionResult Init() noexcept override { init_called_ = true; }

  ExecutionResult Run() noexcept override { run_called_ = true; }

  ExecutionResult Stop() noexcept override { stop_called_ = true; }

  ExecutionResult ResetForwardingAddress(
      const std::string& forwarding_address) noexcept override {
    std::unique_lock lock(mutex_);
    forwarding_address_ = forwarding_address;
    return core::SuccessExecutionResult();
  }

  std::string GetForwardingAddress() {
    std::unique_lock lock(mutex_);
    return forwarding_address_;
  }

  std::atomic<bool> init_called_ = false;
  std::atomic<bool> run_called_ = false;
  std::atomic<bool> stop_called_ = false;

  std::mutex mutex_;
  std::string forwarding_address_ = "";
};
}  // namespace google::scp::core::tcp_traffic_forwarder::mock
