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
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs_common {
class MockConfigProvider : public ConfigProviderInterface {
 public:
  MockConfigProvider() {}

  google::scp::core::ExecutionResult Init() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Run() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Stop() noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      std::string& out) noexcept override {
    if (string_config_map_.find(key) == string_config_map_.end()) {
      return google::scp::core::FailureExecutionResult(
          privacy_sandbox::pbs_common::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = string_config_map_[key];
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      size_t& out) noexcept override {
    if (size_t_config_map_.find(key) == size_t_config_map_.end()) {
      return google::scp::core::FailureExecutionResult(
          privacy_sandbox::pbs_common::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = size_t_config_map_[key];
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      int32_t& out) noexcept override {
    if (int32_t_config_map_.find(key) == int32_t_config_map_.end()) {
      return google::scp::core::FailureExecutionResult(
          privacy_sandbox::pbs_common::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = int32_t_config_map_[key];
    return google::scp::core::SuccessExecutionResult();
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      bool& out) noexcept override {
    if (bool_config_map_.find(key) == bool_config_map_.end()) {
      return google::scp::core::FailureExecutionResult(
          privacy_sandbox::pbs_common::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = bool_config_map_[key];
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      std::list<std::string>& out) noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      std::list<int32_t>& out) noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      std::list<size_t>& out) noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      std::list<bool>& out) noexcept override {
    return google::scp::core::SuccessExecutionResult();
  }

  google::scp::core::ExecutionResult Get(
      const privacy_sandbox::pbs_common::ConfigKey& key,
      double& out) noexcept override {
    if (double_config_map_.find(key) == double_config_map_.end()) {
      return google::scp::core::FailureExecutionResult(
          privacy_sandbox::pbs_common::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
    out = double_config_map_[key];
    return google::scp::core::SuccessExecutionResult();
  }

  void Set(const privacy_sandbox::pbs_common::ConfigKey& key,
           const char* value) {
    string_config_map_[key] = std::string(value);
  }

  void Set(const privacy_sandbox::pbs_common::ConfigKey& key,
           const std::string& value) {
    string_config_map_[key] = value;
  }

  void SetInt(const privacy_sandbox::pbs_common::ConfigKey& key,
              const size_t value) {
    size_t_config_map_[key] = value;
  }

  void SetInt32(const privacy_sandbox::pbs_common::ConfigKey& key,
                const int32_t value) {
    int32_t_config_map_[key] = value;
  }

  void SetBool(const privacy_sandbox::pbs_common::ConfigKey& key,
               const bool value) {
    bool_config_map_[key] = value;
  }

  void SetDouble(const privacy_sandbox::pbs_common::ConfigKey& key,
                 const double value) {
    double_config_map_[key] = value;
  }

 private:
  std::map<privacy_sandbox::pbs_common::ConfigKey, std::string>
      string_config_map_;
  std::map<privacy_sandbox::pbs_common::ConfigKey, size_t> size_t_config_map_;
  std::map<privacy_sandbox::pbs_common::ConfigKey, int32_t> int32_t_config_map_;
  std::map<privacy_sandbox::pbs_common::ConfigKey, bool> bool_config_map_;
  std::map<privacy_sandbox::pbs_common::ConfigKey, double> double_config_map_;
};
}  // namespace privacy_sandbox::pbs_common
