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

#include <string>
#include <string_view>
#include <variant>

#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/common/error_codes.h"

namespace google::scp::core::nosql_database_provider {
class NoSQLDatabaseProviderUtils {
 public:
  /**
   * @brief Creates a NoSQLDatabaseValidAttributeValueTypes from string object
   * and assigns the value.
   *
   * @tparam T The types to parse the value from the string.
   * @param value The input char array value.
   * @param length The length of the char array.
   * @param no_sql_database_valid_attribute The output object to be p
   * @return ExecutionResult The execution result of the operation.
   */
  template <typename T>
  static ExecutionResult FromString(
      const char* value, const size_t length,
      NoSQLDatabaseValidAttributeValueTypes& no_sql_database_valid_attribute) {
    try {
      if (std::is_same<T, int>::value) {
        no_sql_database_valid_attribute = std::stoi(std::string(value, length));
        return SuccessExecutionResult();
      } else if (std::is_same<T, double>::value) {
        no_sql_database_valid_attribute = std::stod(std::string(value, length));
        return SuccessExecutionResult();
      } else if (std::is_same<T, float>::value) {
        no_sql_database_valid_attribute = std::stof(std::string(value, length));
        return SuccessExecutionResult();
      } else if (std::is_same<T, std::string>::value) {
        no_sql_database_valid_attribute = std::string(value, length);
        return SuccessExecutionResult();
      }
    } catch (...) {}

    return FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE);
  }
};
}  // namespace google::scp::core::nosql_database_provider
