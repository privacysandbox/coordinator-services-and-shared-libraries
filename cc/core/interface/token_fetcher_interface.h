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

#include "core/interface/async_context.h"
#include "core/interface/initializable_interface.h"

namespace google::scp::core {

/**
 * @brief generic request object, can contain metadata useful for
 * fetching the token.
 */
struct FetchTokenRequest {};

/**
 * @brief generic response containing queried token and associated metadata
 * @param token fetched token
 * @param token_lifetime_in_seconds duration in seconds of which token is valid
 */
struct FetchTokenResponse {
  Token token;
  std::chrono::seconds token_lifetime_in_seconds;
};

/**
 * @brief An interface for callers to fetch token from another
 * component/service.
 */
class TokenFetcherInterface : public InitializableInterface {
 public:
  /**
   * @brief Fetch the token asynchronously
   * @return ExecutionResult
   */
  virtual ExecutionResult FetchToken(
      AsyncContext<FetchTokenRequest, FetchTokenResponse>) noexcept = 0;

  virtual ~TokenFetcherInterface() = default;
};

}  // namespace google::scp::core
