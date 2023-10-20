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

#include <nlohmann/json.hpp>

#include "cc/core/interface/config_provider_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core {
/*! @copydoc ConfigProviderInterface
 */
class ConfigProvider : public ConfigProviderInterface {
 public:
  /**
   * @brief Constructs a new Config Provider object
   *
   * @param config_file The file path for the configuration .json file.
   */
  explicit ConfigProvider(std::filesystem::path config_file)
      : config_file_(config_file) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult Get(const ConfigKey& key, std::string& out) noexcept override;

  ExecutionResult Get(const ConfigKey& key, int32_t& out) noexcept override;

  ExecutionResult Get(const ConfigKey& key, size_t& out) noexcept override;

  ExecutionResult Get(const ConfigKey& key, bool& out) noexcept override;

  ExecutionResult Get(const ConfigKey& key,
                      std::list<std::string>& out) noexcept override;

  ExecutionResult Get(const ConfigKey& key,
                      std::list<int32_t>& out) noexcept override;

  ExecutionResult Get(const ConfigKey& key,
                      std::list<size_t>& out) noexcept override;

  ExecutionResult Get(const ConfigKey& key,
                      std::list<bool>& out) noexcept override;

 private:
  template <typename T>
  ExecutionResult Get(const ConfigKey& key, T& out) noexcept {
    if (!config_json_.contains(key)) {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    try {
      out = config_json_[key].get<T>();
    } catch (nlohmann::json::exception& e) {
      return FailureExecutionResult(
          errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR);
    }
    return SuccessExecutionResult();
  }

  template <typename T>
  ExecutionResult Get(const ConfigKey& key, std::list<T>& out) noexcept {
    auto it = config_json_.find(key);
    if (it == config_json_.end()) {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }

    try {
      for (auto value : it.value()) {
        out.push_back(value);
      }
    } catch (nlohmann::json::exception& e) {
      return FailureExecutionResult(
          errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR);
    }

    return SuccessExecutionResult();
  }

  /// The file path for the configuration .json file.
  std::filesystem::path config_file_;
  /// The parsed json content.
  nlohmann::json config_json_;
};
}  // namespace google::scp::core
