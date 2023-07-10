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

#include <list>
#include <string>

#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {

/// Configuration key for value retrieval.
typedef std::string ConfigKey;

/**
 * @brief ConfigProvider keeps all the configuration key/values for any given
 * applications and all of its underlying components.
 */
class ConfigProviderInterface : public ServiceInterface {
 public:
  virtual ~ConfigProviderInterface() = default;

  /**
   * @brief Looks up configuration key with string output type. If any error
   * occurs, ExecutionResult can be used to identify the problem.
   * @param key configuration key.
   * @param out configuration value;
   * @return ExecutionResult
   */
  virtual ExecutionResult Get(const ConfigKey& key,
                              std::string& out) noexcept = 0;

  /**
   * @brief Looks up configuration key with int32_t output type. If any error
   * occurs, ExecutionResult can be used to identify the problem.
   * @param key configuration key.
   * @param out configuration value;
   * @return ExecutionResult
   */
  virtual ExecutionResult Get(const ConfigKey& key, int32_t& out) noexcept = 0;

  /**
   * @brief Looks up configuration key with size_t output type. If any error
   * occurs, ExecutionResult can be used to identify the problem.
   * @param key configuration key.
   * @param out configuration value;
   * @return ExecutionResult
   */
  virtual ExecutionResult Get(const ConfigKey& key, size_t& out) noexcept = 0;

  /**
   * @brief Looks up configuration key with bool output type. If any error
   * occurs, ExecutionResult can be used to identify the problem.
   * @param key configuration key.
   * @param out configuration value;
   * @return ExecutionResult
   */
  virtual ExecutionResult Get(const ConfigKey& key, bool& out) noexcept = 0;

  /**
   * @brief Looks up configuration key with list of string output type. If any
   * error occurs, ExecutionResult can be used to identify the problem.
   * @param key configuration key.
   * @param out configuration value;
   * @return ExecutionResult
   */
  virtual ExecutionResult Get(const ConfigKey& key,
                              std::list<std::string>& out) noexcept = 0;

  /**
   * @brief Looks up configuration key with list of int32_t output type. If any
   * error occurs, ExecutionResult can be used to identify the problem.
   * @param key configuration key.
   * @param out configuration value;
   * @return ExecutionResult
   */
  virtual ExecutionResult Get(const ConfigKey& key,
                              std::list<int32_t>& out) noexcept = 0;

  /**
   * @brief Looks up configuration key with list of size_t output type. If any
   * error occurs, ExecutionResult can be used to identify the problem.
   * @param key configuration key.
   * @param out configuration value;
   * @return ExecutionResult
   */
  virtual ExecutionResult Get(const ConfigKey& key,
                              std::list<size_t>& out) noexcept = 0;

  /**
   * @brief Looks up configuration key with list of bool output type. If any
   * error occurs, ExecutionResult can be used to identify the problem.
   * @param key configuration key.
   * @param out configuration value;
   * @return ExecutionResult
   */
  virtual ExecutionResult Get(const ConfigKey& key,
                              std::list<bool>& out) noexcept = 0;
};
}  // namespace google::scp::core
