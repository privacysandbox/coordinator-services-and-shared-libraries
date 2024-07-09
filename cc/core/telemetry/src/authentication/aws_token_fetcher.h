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

#include "cc/core/telemetry/src/authentication/grpc_auth_config.h"
#include "cc/core/telemetry/src/authentication/token_fetcher.h"
#include "cc/public/core/interface/execution_result.h"
#include "google/cloud/iam/credentials/v1/iam_credentials_client.h"

namespace google::scp::core {

/**
 * AwsTokenFetcher is a concrete implementation of the TokenFetcher interface,
 * designed to fetch ID tokens for authentication with Google Cloud Platform
 * (GCP) services.
 *
 * This class utilizes the gRPC authentication configuration to create a client
 * for the IAMCredentials API, which is used to fetch the ID tokens. The fetched
 * tokens can then be used to authenticate gRPC calls to GCP services.
 *
 * For more information:
 * https://cloud.google.com/docs/authentication/token-types#id
 * https://cloud.google.com/docs/authentication/get-id-token
 * https://cloud.google.com/iam/docs/workload-identity-federation-with-other-clouds
 * https://cloud.google.com/cpp/docs/reference/common/2.13.0/namespacegoogle_1_1cloud#group__guac_1ga75e5dbea7079dbb177fe6f7e8bd6edb5
 * https://cloud.google.com/run/docs/authenticating/service-to-service#use_workload_identity_federation_from_outside
 */
class AwsTokenFetcher : public TokenFetcher {
 public:
  ExecutionResultOr<std::string> FetchIdToken(
      GrpcAuthConfig& auth_config) override;

 private:
  std::unique_ptr<::google::cloud::iam_credentials_v1::IAMCredentialsClient>
      iam_client_;
  void CreateIamClient(const GrpcAuthConfig& auth_config);
};
}  // namespace google::scp::core
