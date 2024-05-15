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
#include <map>
#include <memory>
#include <string>

#include "cc/core/telemetry/src/authentication/grpc_auth_config.h"
#include "core/telemetry/src/authentication/token_fetcher.h"
#include "grpcpp/security/credentials.h"

namespace google::scp::core {
/**
 * @brief Periodically refreshes authentication tokens for gRPC metadata.
 *
 * This class manages the automatic refreshing of authentication tokens
 * for gRPC authentication. It abstracts token management complexity,
 * allowing seamless integration with gRPC clients.
 *
 * This class will be initialized with the right implementation of token fetcher
 * and grpc auth config and will be managing the id token for a particular
 * exporter.
 *
 * @note
 * https://grpc.io/docs/guides/auth/#extending-grpc-to-support-other-authentication-mechanisms
 */
class GrpcIdTokenAuthenticator : public grpc::MetadataCredentialsPlugin {
 public:
  GrpcIdTokenAuthenticator() = default;

  explicit GrpcIdTokenAuthenticator(
      std::unique_ptr<GrpcAuthConfig> auth_config,
      std::unique_ptr<TokenFetcher> token_fetcher);

  grpc::Status GetMetadata(
      grpc::string_ref service_url, grpc::string_ref method_name,
      const grpc::AuthContext& channel_auth_context,
      std::multimap<grpc::string, grpc::string>* metadata) override;

  GrpcAuthConfig* auth_config() const;

  // Expiry time of the ID token (for testing purposes)
  void set_expiry_time_for_testing(
      const std::chrono::system_clock::time_point& expiry_time);
  const std::chrono::system_clock::time_point& expiry_time() const;

  // Checks if the token has expired based on the given duration
  bool IsExpired() const;

  const std::string& id_token() const;

  // For testing or storing a generated ID token
  void set_id_token(absl::string_view token);

 private:
  std::unique_ptr<GrpcAuthConfig> auth_config_;
  std::unique_ptr<TokenFetcher> token_fetcher_;
  std::string id_token_;
  std::chrono::system_clock::time_point expiry_time_;
};
}  // namespace google::scp::core
