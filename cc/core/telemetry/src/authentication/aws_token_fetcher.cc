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

#include "aws_token_fetcher.h"

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/telemetry/src/authentication/error_codes.h"
#include "google/cloud/credentials.h"

#include "token_fetcher_utils.h"

namespace google::scp::core {

inline constexpr absl::string_view kAwsTokenFetcher = "AwsTokenFetcher";

/*
 * Details here:
 * https://cloud.google.com/cpp/docs/reference/common/2.13.0/namespacegoogle_1_1cloud#group__guac_1ga75e5dbea7079dbb177fe6f7e8bd6edb5
 * https://cloud.google.com/sdk/gcloud/reference/iam/workload-identity-pools/create-cred-config
 */

ExecutionResultOr<std::string> AwsTokenFetcher::FetchIdToken(
    GrpcAuthConfig& auth_config) {
  CreateIamClient(auth_config);

  auto execution_result = FetchIdTokenInternal(*iam_client_, auth_config);
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsTokenFetcher, google::scp::core::common::kZeroUuid,
              execution_result.result(),
              "[Aws Token Fetch] Id token fetch failed: %s",
              google::scp::core::errors::GetErrorMessage(
                  execution_result.result().status_code));
  }
  return execution_result;
}

void AwsTokenFetcher::CreateIamClient(const GrpcAuthConfig& auth_config) {
  std::shared_ptr<google::cloud::iam_credentials_v1::IAMCredentialsConnection>
      credentials_connection;

  if (auth_config.cred_config().empty()) {
    credentials_connection =
        cloud::iam_credentials_v1::MakeIAMCredentialsConnection(
            google::cloud::Options());
  } else {
    credentials_connection =
        cloud::iam_credentials_v1::MakeIAMCredentialsConnection(
            google::cloud::Options()
                .set<google::cloud::UnifiedCredentialsOption>(
                    google::cloud::MakeExternalAccountCredentials(
                        std::string(auth_config.cred_config()))));
  }

  iam_client_ =
      std::make_unique<cloud::iam_credentials_v1::IAMCredentialsClient>(
          credentials_connection);
}
}  // namespace google::scp::core
