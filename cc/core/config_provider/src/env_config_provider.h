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

#include <cstdlib>
#include <list>
#include <sstream>
#include <string>
#include <type_traits>

#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/utils/src/string_util.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core {

static constexpr char kCommaDelimiter[] = ",";

/*! @copydoc ConfigProviderInterface
 */
class EnvConfigProvider : public ConfigProviderInterface {
 public:
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
    const char* var_value = std::getenv(key.c_str());

    if (var_value == NULL) {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }

    const std::string var_value_string(var_value);
    auto result = StringToType(var_value_string, out);
    if (!result.Successful()) {
      return result;
    }

    return SuccessExecutionResult();
  }

  /**
   * @brief Get a list of items out of an enironment variables.
   * The expected format is that the items are a single string, separated
   * by a comma. Since comma is used as a delimiter, it cannot be part of the
   * values.
   * Example of a variable that would be parsed out to a list:
   * Single string "1,2,3,4" would be turned into ["1","2","3","4"] if string.
   *
   * @tparam T The type to convert the list items to.
   * @param key The configuration (environment variable) name.
   * @param out The parsed list.
   * @return ExecutionResult Failure if the list cant't be parsed, or the config
   * does not exist.
   */
  template <typename T>
  ExecutionResult Get(const ConfigKey& key, std::list<T>& out) noexcept {
    const char* var_value = std::getenv(key.c_str());

    if (var_value == NULL) {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }

    std::list<std::string> parts;
    const std::string value(var_value);
    const std::string delimiter(kCommaDelimiter);
    utils::SplitStringByDelimiter(value, delimiter, parts);

    for (const auto& part : parts) {
      T out_part;

      auto result = StringToType(part, out_part);
      if (!result.Successful()) {
        return result;
      }

      out.push_back(out_part);
    }

    return SuccessExecutionResult();
  }

  /**
   * @brief Convert a string to the desired, supported type.
   *
   * @tparam T the desired output type.
   * @param str The input string.
   * @param out The output value.
   * @return ExecutionResult failure if the conversion failed.
   */
  template <typename T>
  ExecutionResult StringToType(const std::string& str, T& out) {
    std::stringstream string_stream;
    if (std::is_same<T, bool>::value) {
      string_stream << std::boolalpha << str;
      string_stream >> std::boolalpha >> out;
    } else {
      string_stream << str;
      string_stream >> out;
    }

    if (std::is_same<T, std::string>::value && str.empty()) {
      // We only allow an empty value for the string type.
      // For other types, we let the stringstream mark
      // the operation as a failure.
      string_stream.clear();
    }

    if (string_stream.fail()) {
      return FailureExecutionResult(
          errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR);
    }

    return SuccessExecutionResult();
  }
};
}  // namespace google::scp::core
