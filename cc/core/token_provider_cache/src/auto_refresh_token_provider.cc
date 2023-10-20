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
#include "auto_refresh_token_provider.h"

#include <string>

#include "core/interface/async_context.h"
#include "core/interface/token_fetcher_interface.h"

#include "error_codes.h"

using google::scp::core::FetchTokenRequest;
using google::scp::core::FetchTokenResponse;
using google::scp::core::common::TimeProvider;
using std::make_shared;
using std::shared_lock;
using std::shared_ptr;
using std::string;
using std::unique_lock;
using std::unique_ptr;
using std::chrono::seconds;
using std::placeholders::_1;

static constexpr const char kAutoRefreshTokenProvider[] =
    "AutoRefreshTokenProvider";

namespace google::scp::core {
ExecutionResult AutoRefreshTokenProviderService::Init() noexcept {
  RETURN_IF_FAILURE(token_fetcher_->Init());
  return SuccessExecutionResult();
}

ExecutionResult AutoRefreshTokenProviderService::Run() noexcept {
  return RefreshToken();
}

ExecutionResult AutoRefreshTokenProviderService::Stop() noexcept {
  if (async_task_canceller_ && !async_task_canceller_()) {
    return FailureExecutionResult(
        errors::SC_AUTO_REFRESH_TOKEN_PROVIDER_FAILED_TO_STOP);
  }
  return SuccessExecutionResult();
}

ExecutionResultOr<shared_ptr<Token>>
AutoRefreshTokenProviderService::GetToken() noexcept {
  shared_lock lock(mutex_);
  if (!cached_token_) {
    return FailureExecutionResult(
        errors::SC_AUTO_REFRESH_TOKEN_PROVIDER_TOKEN_NOT_AVAILABLE);
  }
  return cached_token_;
}

ExecutionResult AutoRefreshTokenProviderService::RefreshToken() {
  AsyncContext<FetchTokenRequest, FetchTokenResponse> get_token_context(
      make_shared<FetchTokenRequest>(),
      bind(&AutoRefreshTokenProviderService::OnRefreshTokenCallback, this, _1));
  auto execution_result = token_fetcher_->FetchToken(get_token_context);
  if (!execution_result.Successful()) {
    SCP_CRITICAL_CONTEXT(
        kAutoRefreshTokenProvider, get_token_context, execution_result,
        "Cannot query token, resetting cached token to empty and "
        "rescheduling RefreshToken");
    // Reset the cached token
    {
      unique_lock lock(mutex_);
      cached_token_ = nullptr;
    }
    // Schedule Refresh for later
    return async_executor_->ScheduleFor(
        [this]() { RefreshToken(); },
        (TimeProvider::GetSteadyTimestampInNanoseconds() +
         std::chrono::seconds(1))
            .count(),
        async_task_canceller_);
  }
  return execution_result;
}

void AutoRefreshTokenProviderService::OnRefreshTokenCallback(
    AsyncContext<FetchTokenRequest, FetchTokenResponse>& get_token_context) {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAutoRefreshTokenProvider, get_token_context,
                      get_token_context.result,
                      "Cannot query token, resetting cached token to empty and "
                      "rescheduling RefreshToken");
    // Reset the cached token
    {
      unique_lock lock(mutex_);
      cached_token_ = nullptr;
    }
    // Schedule Refresh for later
    auto execution_result = async_executor_->ScheduleFor(
        [this]() { RefreshToken(); },
        (TimeProvider::GetSteadyTimestampInNanoseconds() +
         std::chrono::seconds(1))
            .count(),
        async_task_canceller_);
    if (!execution_result.Successful()) {
      SCP_CRITICAL_CONTEXT(
          kAutoRefreshTokenProvider, get_token_context, execution_result,
          "Cannot schedule RefreshToken. External requests will "
          "start to fail!");
    }
    return;
  }

  // Cache the fetched token
  {
    unique_lock lock(mutex_);
    cached_token_ = make_shared<string>(get_token_context.response->token);
  }

  SCP_INFO_CONTEXT(kAutoRefreshTokenProvider, get_token_context,
                   "Token Refreshed");

  auto execution_result = async_executor_->ScheduleFor(
      [this]() { RefreshToken(); },
      (TimeProvider::GetSteadyTimestampInNanoseconds() +
       get_token_context.response->token_lifetime_in_seconds)
          .count(),
      async_task_canceller_);
  if (!execution_result.Successful()) {
    SCP_CRITICAL_CONTEXT(kAutoRefreshTokenProvider, get_token_context,
                         execution_result,
                         "Cannot schedule RefreshToken. External requests will "
                         "start to fail!");
  }
}

}  // namespace google::scp::core
