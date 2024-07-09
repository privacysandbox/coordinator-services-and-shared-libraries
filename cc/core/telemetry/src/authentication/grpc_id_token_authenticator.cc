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

#include "grpc_id_token_authenticator.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "core/telemetry/src/authentication/gcp_token_fetcher.h"

namespace google::scp::core {

// Token validity
// Defaults to 1hr
// https://cloud.google.com/docs/authentication/token-types#id-lifetime
inline constexpr std::int32_t kIdTokenValidity = 3000;

/**
 * @brief Periodically refreshes authentication tokens for gRPC metadata.
 *
 * This class manages the automatic refreshing of authentication tokens
 * for gRPC authentication. It abstracts token management complexity,
 * allowing seamless integration with gRPC clients.
 *
 * @note
 * https://grpc.io/docs/guides/auth/#extending-grpc-to-support-other-authentication-mechanisms
 */
GrpcIdTokenAuthenticator::GrpcIdTokenAuthenticator(
    std::unique_ptr<GrpcAuthConfig> auth_config,
    std::unique_ptr<TokenFetcher> token_fetcher)
    : auth_config_(std::move(auth_config)),
      token_fetcher_(std::move(token_fetcher)),
      expiry_time_(std::chrono::system_clock::now()) {}

grpc::Status GrpcIdTokenAuthenticator::GetMetadata(
    grpc::string_ref service_url, grpc::string_ref method_name,
    const grpc::AuthContext& channel_auth_context,
    std::multimap<grpc::string, grpc::string>* metadata) {
  // handling token expiry
  if (IsExpired()) {
    ExecutionResultOr<std::string> token =
        token_fetcher_->FetchIdToken(*auth_config_);
    if (!token.Successful()) {
      return grpc::Status(grpc::StatusCode::UNKNOWN,
                          "token_fetcher_->FetchIdToken() failed");
    }
    id_token_ = *token;
    expiry_time_ = std::chrono::system_clock::now() +
                   std::chrono::seconds(kIdTokenValidity);
  }

  metadata->insert(std::make_pair("authorization", "Bearer " + id_token_));
  return grpc::Status::OK;
}

GrpcAuthConfig* GrpcIdTokenAuthenticator::auth_config() const {
  return auth_config_.get();
}

void GrpcIdTokenAuthenticator::set_id_token(absl::string_view token) {
  id_token_ = token;
}

const std::string& GrpcIdTokenAuthenticator::id_token() const {
  return id_token_;
}

void GrpcIdTokenAuthenticator::set_expiry_time_for_testing(
    const std::chrono::system_clock::time_point& expiry_time) {
  expiry_time_ = expiry_time;
}

const std::chrono::system_clock::time_point&
GrpcIdTokenAuthenticator::expiry_time() const {
  return expiry_time_;
}

bool GrpcIdTokenAuthenticator::IsExpired() const {
  return std::chrono::system_clock::now() >= expiry_time_;
}
}  // namespace google::scp::core
