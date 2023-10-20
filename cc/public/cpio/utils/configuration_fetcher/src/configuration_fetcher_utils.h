/*
 * Copyright 2023 Google LLC
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
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/configuration_fetcher/interface/configuration_fetcher_interface.h"

#include "error_codes.h"

namespace google::scp::cpio {

static constexpr char kConfigurationFetcherUtils[] =
    "ConfigurationFetcherUtils";

class ConfigurationFetcherUtils {
 public:
  // The RequestT or ResponseT cannot be void.
  template <typename ParameterValueT>
  static core::AsyncContext<std::string, std::string> ContextConvertCallback(
      const std::string& parameter_name,
      core::AsyncContext<GetConfigurationRequest, ParameterValueT>&
          context_without_parameter_name,
      const std::function<core::ExecutionResultOr<ParameterValueT>(
          const std::string&)>& convert_func) noexcept {
    return core::AsyncContext<std::string, std::string>(
        std::make_shared<std::string>(parameter_name),
        [context_without_parameter_name, convert_func](
            core::AsyncContext<std::string, std::string> context) mutable {
          context_without_parameter_name.result = context.result;
          if (context.result.Successful()) {
            auto convert_result = convert_func(*context.response);
            if (convert_result.Successful()) {
              context_without_parameter_name.response =
                  std::make_shared<ParameterValueT>(convert_result.release());
            } else {
              context_without_parameter_name.result = convert_result.result();
            }
          }
          context_without_parameter_name.Finish();
        });
  }

  template <typename UIntT>
  static core::ExecutionResultOr<UIntT> StringToUInt(const std::string& value) {
    try {
      // Avoid converting negative numbers using stoull because stoull will
      // convert it to positive numbers automatically.
      if (value.size() > 0 && value[0] == '-') {
        auto result = core::FailureExecutionResult(
            core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED);
        SCP_ERROR(kConfigurationFetcherUtils, core::common::kZeroUuid, result,
                  "Could not convert parameter value: %s", value.c_str());
        return static_cast<core::ExecutionResult>(result);
      }
      auto int_value = std::stoull(value);
      if (int_value < std::numeric_limits<UIntT>::min() ||
          int_value > std::numeric_limits<UIntT>::max()) {
        auto result = core::FailureExecutionResult(
            core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED);
        SCP_ERROR(kConfigurationFetcherUtils, core::common::kZeroUuid, result,
                  "Could not convert parameter value: %s", value.c_str());
        return static_cast<core::ExecutionResult>(result);
      }
      return static_cast<UIntT>(int_value);
    } catch (...) {
      auto result = core::FailureExecutionResult(
          core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED);
      SCP_ERROR(kConfigurationFetcherUtils, core::common::kZeroUuid, result,
                "Could not convert parameter value to integer: %s",
                value.c_str());
      return static_cast<core::ExecutionResult>(result);
    }
  }

  template <typename EnumT>
  static core::ExecutionResultOr<EnumT> StringToEnum(
      const std::string& value, const std::map<std::string, EnumT>& enum_map) {
    auto it = enum_map.find(value);
    if (it == enum_map.end()) {
      auto result = core::FailureExecutionResult(
          core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED);
      SCP_ERROR(kConfigurationFetcherUtils, core::common::kZeroUuid, result,
                "Could not convert %s to enum", value.c_str());
      return static_cast<core::ExecutionResult>(result);
    }
    return it->second;
  }
};
}  // namespace google::scp::cpio
