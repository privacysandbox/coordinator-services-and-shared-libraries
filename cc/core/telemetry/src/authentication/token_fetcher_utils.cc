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

#include "token_fetcher_utils.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "cc/core/telemetry/src/authentication/grpc_auth_config.h"
#include "core/common/uuid/src/uuid.h"
#include "core/telemetry/src/authentication/error_codes.h"

namespace google::scp::core {

/*
 * The resource name of the service account for which the credentials are
 * requested, in the following format:
 * projects/-/serviceAccounts/{ACCOUNT_EMAIL_OR_UNIQUEID}. The - wildcard
 * character is required; replacing it with a project ID is invalid.
 */
ExecutionResultOr<std::string> FetchIdTokenInternal(
    google::cloud::iam_credentials_v1::IAMCredentialsClient& iam_client,
    const GrpcAuthConfig& auth_config) {
  const std::string request_name =
      absl::StrCat("projects/-/serviceAccounts/",
                   std::string(auth_config.service_account()));
  const std::vector<std::string> delegates;
  cloud::StatusOr<google::iam::credentials::v1::GenerateIdTokenResponse>
      response = iam_client.GenerateIdToken(
          request_name, delegates, std::string(auth_config.audience()), true);
  if (!response) {
    return FailureExecutionResult(
        SC_TELEMETRY_AUTHENTICATION_ID_TOKEN_FETCH_FAILED);
  }
  return response->token();
}
}  // namespace google::scp::core
