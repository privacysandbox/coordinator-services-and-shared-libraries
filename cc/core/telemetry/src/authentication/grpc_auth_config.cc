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

#include "grpc_auth_config.h"

#include <utility>

#include "core/telemetry/src/common/telemetry_configuration.h"

namespace google::scp::core {
GrpcAuthConfig::GrpcAuthConfig(std::string service_account,
                               std::string audience,
                               std::string cred_config /* = "" */)
    : service_account_(std::move(service_account)),
      audience_(std::move(audience)),
      cred_config_(std::move(cred_config)) {}

GrpcAuthConfig::GrpcAuthConfig(ConfigProviderInterface& config_provider) {
  service_account_ =
      GetConfigValue(std::string(kOtelServiceAccountKey),
                     std::string(kOtelServiceAccountValue), config_provider);
  audience_ = GetConfigValue(std::string(kOtelAudienceKey),
                             std::string(kOtelAudienceValue), config_provider);
  cred_config_ =
      GetConfigValue(std::string(kOtelCredConfigKey),
                     std::string(kOtelCredConfigValue), config_provider);
}

absl::string_view GrpcAuthConfig::service_account() const {
  return service_account_;
}

absl::string_view GrpcAuthConfig::audience() const {
  return audience_;
}

absl::string_view GrpcAuthConfig::cred_config() const {
  return cred_config_;
}

bool GrpcAuthConfig::IsValid() const {
  if (service_account_.empty() || audience_.empty()) {
    return false;
  }
  return true;
}
}  // namespace google::scp::core
