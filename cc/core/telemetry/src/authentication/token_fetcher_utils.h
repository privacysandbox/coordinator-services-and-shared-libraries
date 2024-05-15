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

#ifndef CC_CORE_TELEMETRY_SRC_AUTHENTICATION_TOKEN_FETCHER_UTILS
#define CC_CORE_TELEMETRY_SRC_AUTHENTICATION_TOKEN_FETCHER_UTILS

#include <string>

#include "cc/core/telemetry/src/authentication/grpc_auth_config.h"
#include "cc/core/telemetry/src/authentication/token_fetcher.h"
#include "cc/public/core/interface/execution_result.h"
#include "google/cloud/iam/credentials/v1/iam_credentials_client.h"

namespace google::scp::core {

ExecutionResultOr<std::string> FetchIdTokenInternal(
    google::cloud::iam_credentials_v1::IAMCredentialsClient& iam_client,
    const GrpcAuthConfig& auth_config);

}  // namespace google::scp::core

#endif
