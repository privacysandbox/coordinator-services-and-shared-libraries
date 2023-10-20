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
#include <map>
#include <string>

#include "cc/core/config_provider/src/error_codes.h"
#include "cc/core/interface/config_provider_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::config_provider::mock {
class MockConfigProvider : public ConfigProviderInterface {
 public:
  MockConfigProvider() {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Get(const ConfigKey& key,
                      std::string& out) noexcept override {
    if (string_config_map_.find(key) == string_config_map_.end()) {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = string_config_map_[key];
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key, size_t& out) noexcept override {
    if (size_t_config_map_.find(key) == size_t_config_map_.end()) {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = size_t_config_map_[key];
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key, int32_t& out) noexcept override {
    if (int32_t_config_map_.find(key) == int32_t_config_map_.end()) {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = int32_t_config_map_[key];
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key, bool& out) noexcept override {
    if (bool_config_map_.find(key) == bool_config_map_.end()) {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = bool_config_map_[key];
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      std::list<std::string>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      std::list<int32_t>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      std::list<size_t>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      std::list<bool>& out) noexcept override {
    return SuccessExecutionResult();
  }

  void Set(const ConfigKey& key, const char* value) {
    string_config_map_[key] = std::string(value);
  }

  void Set(const ConfigKey& key, const std::string& value) {
    string_config_map_[key] = value;
  }

  void SetInt(const ConfigKey& key, const size_t value) {
    size_t_config_map_[key] = value;
  }

  void SetBool(const ConfigKey& key, const bool value) {
    bool_config_map_[key] = value;
  }

 private:
  std::map<ConfigKey, std::string> string_config_map_;
  std::map<ConfigKey, size_t> size_t_config_map_;
  std::map<ConfigKey, int32_t> int32_t_config_map_;
  std::map<ConfigKey, bool> bool_config_map_;
};
}  // namespace google::scp::core::config_provider::mock
