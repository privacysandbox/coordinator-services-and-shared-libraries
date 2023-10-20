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
// This file defines a signer of the client for aws authorizer. Note that this
// not the AWS SigV4 or Signature Version 4 implementation, but a utility to
// utilize the SigV4 standard to do pre-signing in our customized token format.
// Clients that uses this signer will be able to call services that ues the
// AwsAuthorizer to authorize.

#include <string>

#include "core/http2_client/src/aws/aws_v4_signer.h"

namespace google::scp::core {
class AwsAuthorizerClientSigner {
 public:
  AwsAuthorizerClientSigner(const std::string& aws_access_key,
                            const std::string& aws_secret_key,
                            const std::string& aws_security_token,
                            const std::string& aws_region);

  /**
   * @brief Create an auth token with target auth relay endpoint.
   *
   * @param endpoint[in] The auth relay endpoint, namely the API gateway
   * endpoint we expect the sever to call in order to verify the authN/authZ.
   * @param token[out] The generate token.
   * @return ExecutionResult
   */
  ExecutionResult GetAuthToken(const std::string& endpoint, std::string& token);

  /**
   * @brief Sign the request by calculating the signature for calling the auth
   * relay.
   *
   * @param http_request[out] The request to sign. A authorization header will
   * be added to this request upon success.
   * @param endpoint[in] The auth relay endpoint, namely the API gateway
   * endpoint we expect the sever to call in order to verify the authN/authZ.
   * @return ExecutionResult
   */
  ExecutionResult SignRequest(HttpRequest& http_request,
                              const std::string& endpoint);

 private:
  AwsV4Signer aws_v4_signer_;
};
}  // namespace google::scp::core
