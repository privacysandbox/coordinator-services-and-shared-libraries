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

#include "env_config_provider.h"

#include <list>
#include <string>

using std::list;
using std::string;

namespace google::scp::core {

ExecutionResult EnvConfigProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult EnvConfigProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult EnvConfigProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult EnvConfigProvider::Get(const ConfigKey& key,
                                       int32_t& out) noexcept {
  return Get<int32_t>(key, out);
}

ExecutionResult EnvConfigProvider::Get(const ConfigKey& key,
                                       size_t& out) noexcept {
  return Get<size_t>(key, out);
}

ExecutionResult EnvConfigProvider::Get(const ConfigKey& key,
                                       string& out) noexcept {
  return Get<string>(key, out);
}

ExecutionResult EnvConfigProvider::Get(const ConfigKey& key,
                                       bool& out) noexcept {
  return Get<bool>(key, out);
}

ExecutionResult EnvConfigProvider::Get(const ConfigKey& key,
                                       list<string>& out) noexcept {
  return Get<string>(key, out);
}

ExecutionResult EnvConfigProvider::Get(const ConfigKey& key,
                                       list<int32_t>& out) noexcept {
  return Get<int32_t>(key, out);
}

ExecutionResult EnvConfigProvider::Get(const ConfigKey& key,
                                       list<size_t>& out) noexcept {
  return Get<size_t>(key, out);
}

ExecutionResult EnvConfigProvider::Get(const ConfigKey& key,
                                       list<bool>& out) noexcept {
  return Get<bool>(key, out);
}
}  // namespace google::scp::core
