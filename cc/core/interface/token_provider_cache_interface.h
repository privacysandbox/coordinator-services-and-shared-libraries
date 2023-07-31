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

#include "core/interface/service_interface.h"

namespace google::scp::core {

/**
 * @brief Interface to provide tokens from a cache
 */
class TokenProviderCacheInterface : public ServiceInterface {
 public:
  /**
   * @brief Get cached token
   * Returns a cheap to copy and reusable token string
   * @return ExecutionResultOr<std::shared_ptr<Token>> token string
   */
  virtual ExecutionResultOr<std::shared_ptr<Token>> GetToken() noexcept = 0;

  virtual ~TokenProviderCacheInterface() = default;
};

}  // namespace google::scp::core
