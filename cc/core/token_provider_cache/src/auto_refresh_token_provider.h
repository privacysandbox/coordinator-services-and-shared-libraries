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

#include <memory>
#include <shared_mutex>
#include <utility>

#include "core/interface/async_executor_interface.h"
#include "core/interface/token_fetcher_interface.h"
#include "core/interface/token_provider_cache_interface.h"

namespace google::scp::core {

class AutoRefreshTokenProviderService : public TokenProviderCacheInterface {
 public:
  AutoRefreshTokenProviderService(
      std::unique_ptr<TokenFetcherInterface> token_fetcher,
      std::shared_ptr<AsyncExecutorInterface> async_executor)
      : token_fetcher_(std::move(token_fetcher)),
        async_executor_(async_executor) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResultOr<std::shared_ptr<Token>> GetToken() noexcept override;

 protected:
  /**
   * @brief Refresh Token helper
   *
   * @return ExecutionResult
   */
  ExecutionResult RefreshToken();

  /**
   * @brief Callback after fetching token from remote
   */
  void OnRefreshTokenCallback(
      AsyncContext<FetchTokenRequest, FetchTokenResponse>&);

  /// @brief Interface object to query token from remote
  std::unique_ptr<TokenFetcherInterface> token_fetcher_;

  /// @brief Async executor to execute token query in async manner
  std::shared_ptr<AsyncExecutorInterface> async_executor_;

  /// @brief Token query async task cancel lambda
  std::function<bool()> async_task_canceller_;

  /// @brief Token fetched using token fetcher
  std::shared_ptr<Token> cached_token_;

  /// @brief Mutex to protect the cached token
  std::shared_mutex mutex_;
};
}  // namespace google::scp::core
