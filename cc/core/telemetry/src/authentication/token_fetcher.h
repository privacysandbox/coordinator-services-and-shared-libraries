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
#ifndef CC_CORE_TELEMETRY_SRC_AUTHENTICATION_TOKEN_FETCHER
#define CC_CORE_TELEMETRY_SRC_AUTHENTICATION_TOKEN_FETCHER

#include <chrono>
#include <memory>
#include <string>

#include <public/core/interface/execution_result.h>

#include "grpcpp/impl/status.h"

namespace google::scp::core {

/**
 * TokenFetcher is an abstract base class that defines an interface for
 * fetching ID tokens used for authentication purposes. Implementations of
 * this class are responsible for providing the mechanism to obtain these
 * tokens from the appropriate source.
 */
class TokenFetcher {
 public:
  virtual ~TokenFetcher() = default;

  // Fetches Id token for authentication.
  virtual ExecutionResultOr<std::string> FetchIdToken(
      GrpcAuthConfig& auth_config) = 0;
};
}  // namespace google::scp::core
#endif
