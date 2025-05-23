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

#include <memory>
#include <string>

#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/errors.h"
#include "google/cloud/credentials.h"

#include "token_fetcher_utils.h"

namespace privacy_sandbox::pbs_common {

using ::google::cloud::MakeExternalAccountCredentials;
using ::google::cloud::Options;
using ::google::cloud::UnifiedCredentialsOption;
using ::google::cloud::iam_credentials_v1::IAMCredentialsClient;
using ::google::cloud::iam_credentials_v1::IAMCredentialsConnection;
using ::google::cloud::iam_credentials_v1::MakeIAMCredentialsConnection;

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
    SCP_ERROR(kAwsTokenFetcher, kZeroUuid, execution_result.result(),
              "[Aws Token Fetch] Id token fetch failed: %s",
              GetErrorMessage(execution_result.result().status_code));
  }
  return execution_result;
}

void AwsTokenFetcher::CreateIamClient(const GrpcAuthConfig& auth_config) {
  std::shared_ptr<IAMCredentialsConnection> credentials_connection;

  if (auth_config.cred_config().empty()) {
    credentials_connection = MakeIAMCredentialsConnection(Options());
  } else {
    credentials_connection = MakeIAMCredentialsConnection(
        Options().set<UnifiedCredentialsOption>(MakeExternalAccountCredentials(
            std::string(auth_config.cred_config()))));
  }

  iam_client_ = std::make_unique<IAMCredentialsClient>(credentials_connection);
}
}  // namespace privacy_sandbox::pbs_common
