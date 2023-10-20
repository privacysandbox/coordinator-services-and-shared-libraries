
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

#include <functional>
#include <memory>
#include <string>

#include "core/interface/token_provider_cache_interface.h"

namespace google::scp::core::token_provider_cache::mock {
class DummyTokenProviderCache : public TokenProviderCacheInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResultOr<std::shared_ptr<Token>> GetToken() noexcept override {
    return std::make_shared<std::string>("dummy_token_for_test_only");
  }
};
}  // namespace google::scp::core::token_provider_cache::mock
