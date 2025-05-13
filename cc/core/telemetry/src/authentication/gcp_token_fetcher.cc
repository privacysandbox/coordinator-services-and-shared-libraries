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

#include "gcp_token_fetcher.h"

#include <memory>

#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/errors.h"

#include "token_fetcher_utils.h"

namespace privacy_sandbox::pbs_common {

inline constexpr absl::string_view kGcpTokenFetcher = "GcpTokenFetcher";

ExecutionResultOr<std::string> GcpTokenFetcher::FetchIdToken(
    GrpcAuthConfig& auth_config) {
  CreateIamClient();
  auto execution_result = FetchIdTokenInternal(*iam_client_, auth_config);
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpTokenFetcher, kZeroUuid, execution_result.result(),
              "FetchIdToken() ID Token fetch failed: %s",
              GetErrorMessage(execution_result.result().status_code));
  }
  return execution_result;
}

void GcpTokenFetcher::CreateIamClient() {
  std::shared_ptr<google::cloud::iam_credentials_v1::IAMCredentialsConnection>
      credentials_connection =
          google::cloud::iam_credentials_v1::MakeIAMCredentialsConnection();
  iam_client_ =
      std::make_unique<google::cloud::iam_credentials_v1::IAMCredentialsClient>(
          credentials_connection);
}
}  // namespace privacy_sandbox::pbs_common
