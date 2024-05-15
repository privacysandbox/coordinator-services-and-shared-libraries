//  Copyright 2024 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "core/interface/config_provider_interface.h"

namespace google::scp::core {

class GrpcAuthConfig {
 public:
  GrpcAuthConfig() = default;

  GrpcAuthConfig(std::string service_account, std::string audience,
                 std::string cred_config = "");

  explicit GrpcAuthConfig(ConfigProviderInterface& config_provider);

  // Common for both GCP and AWS
  absl::string_view service_account() const;

  absl::string_view audience() const;

  // For AWS
  absl::string_view cred_config() const;

  // Validates the configuration
  bool IsValid() const;

 private:
  std::string service_account_;
  std::string audience_;
  std::string cred_config_;
};
}  // namespace google::scp::core
