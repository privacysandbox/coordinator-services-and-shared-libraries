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

#include "cc/core/config_provider/src/config_provider.h"

#include <fstream>
#include <list>
#include <string>

using json = nlohmann::json;

namespace privacy_sandbox::pbs_common {
namespace {}  // namespace

ExecutionResult ConfigProvider::Init() noexcept {
  try {
    std::ifstream jsonFile(config_file_);
    config_json_ = json::parse(jsonFile);
  } catch (json::parse_error& e) {
    return FailureExecutionResult(SC_CONFIG_PROVIDER_CANNOT_PARSE_CONFIG_FILE);
  }
  return SuccessExecutionResult();
};

ExecutionResult ConfigProvider::Run() noexcept {
  return SuccessExecutionResult();
};

ExecutionResult ConfigProvider::Stop() noexcept {
  return SuccessExecutionResult();
};

ExecutionResult ConfigProvider::Get(const ConfigKey& key,
                                    int32_t& out) noexcept {
  return Get<int32_t>(key, out);
};

ExecutionResult ConfigProvider::Get(const ConfigKey& key,
                                    size_t& out) noexcept {
  return Get<size_t>(key, out);
};

ExecutionResult ConfigProvider::Get(const ConfigKey& key,
                                    std::string& out) noexcept {
  return Get<std::string>(key, out);
};

ExecutionResult ConfigProvider::Get(const ConfigKey& key, bool& out) noexcept {
  return Get<bool>(key, out);
}

ExecutionResult ConfigProvider::Get(const ConfigKey& key,
                                    double& out) noexcept {
  return Get<double>(key, out);
};

ExecutionResult ConfigProvider::Get(const ConfigKey& key,
                                    std::list<std::string>& out) noexcept {
  return Get<std::string>(key, out);
};

ExecutionResult ConfigProvider::Get(const ConfigKey& key,
                                    std::list<int32_t>& out) noexcept {
  return Get<int32_t>(key, out);
};

ExecutionResult ConfigProvider::Get(const ConfigKey& key,
                                    std::list<size_t>& out) noexcept {
  return Get<size_t>(key, out);
};

ExecutionResult ConfigProvider::Get(const ConfigKey& key,
                                    std::list<bool>& out) noexcept {
  return Get<bool>(key, out);
};
}  // namespace privacy_sandbox::pbs_common
