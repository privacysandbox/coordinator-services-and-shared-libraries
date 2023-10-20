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
#include <sstream>
#include <string>
#include <variant>

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>

#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "core/nosql_database_provider/src/common/nosql_database_provider_utils.h"

namespace google::scp::core::nosql_database_provider {
/**
 * @brief Provides utility functions for AWS DynamoDB request flows. Aws uses
 * custom types that need to be converted to SCP types during runtime.
 */
class AwsDynamoDBUtils {
 public:
  /**
   * @brief Converts DynamoDB types to NoSQLDatabase attribute types.
   *
   * @param attribute_value The value of the attribute to be converted.
   * @param out_value The NoSQLDatabase valid typed value created from the
   * conversion.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult
  ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueType(
      const Aws::DynamoDB::Model::AttributeValue& attribute_value,
      NoSQLDatabaseValidAttributeValueTypes& out_value) {
    try {
      auto source_type = attribute_value.GetType();
      if (source_type == Aws::DynamoDB::Model::ValueType::NUMBER) {
        Aws::String number = attribute_value.GetN();
        return nosql_database_provider::NoSQLDatabaseProviderUtils::FromString<
            double>(number.c_str(), number.length(), out_value);
      }
      if (source_type == Aws::DynamoDB::Model::ValueType::STRING) {
        Aws::String str = attribute_value.GetS();
        out_value = NoSQLDatabaseValidAttributeValueTypes(str.c_str());
        return SuccessExecutionResult();
      }
    } catch (...) {}

    return core::FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE);
  }

  /**
   * @brief Converts NoSQLDatabase attribute types to DynamoDB type.
   *
   * @param value The type of the attribute to be converted.
   * @param out_value The AWS Attribute created from the conversion.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult
  ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
      const NoSQLDatabaseValidAttributeValueTypes& value,
      Aws::DynamoDB::Model::AttributeValue& out_value) {
    try {
      if (std::holds_alternative<int>(value)) {
        out_value.SetN(std::get<int>(value));
        return SuccessExecutionResult();
      } else if (std::holds_alternative<double>(value)) {
        out_value.SetN(std::get<double>(value));
        return SuccessExecutionResult();
      } else if (std::holds_alternative<float>(value)) {
        out_value.SetN(std::get<float>(value));
        return SuccessExecutionResult();
      } else if (std::holds_alternative<std::string>(value)) {
        auto char_array = std::get<std::string>(value).c_str();
        out_value.SetS(char_array);
        return SuccessExecutionResult();
      }
    } catch (...) {}

    return core::FailureExecutionResult(
        errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE);
  }

  /**
   * @brief Converts DynamoDB errors to ExecutionResult.
   *
   * @param dynamo_db_error DynamoDB error codes.
   * @return core::ExecutionResult The DynamoDB error code converted to the
   * execution result.
   */
  static core::ExecutionResult ConvertDynamoErrorToExecutionResult(
      const Aws::DynamoDB::DynamoDBErrors& dynamo_db_error) noexcept {
    switch (dynamo_db_error) {
      case Aws::DynamoDB::DynamoDBErrors::INTERNAL_FAILURE:
      case Aws::DynamoDB::DynamoDBErrors::SERVICE_UNAVAILABLE:
      case Aws::DynamoDB::DynamoDBErrors::THROTTLING:
      case Aws::DynamoDB::DynamoDBErrors::SLOW_DOWN:
      case Aws::DynamoDB::DynamoDBErrors::REQUEST_TIMEOUT:
      case Aws::DynamoDB::DynamoDBErrors::PROVISIONED_THROUGHPUT_EXCEEDED:
      case Aws::DynamoDB::DynamoDBErrors::REQUEST_LIMIT_EXCEEDED:
      case Aws::DynamoDB::DynamoDBErrors::RESOURCE_IN_USE:
      case Aws::DynamoDB::DynamoDBErrors::TABLE_IN_USE:
      case Aws::DynamoDB::DynamoDBErrors::TRANSACTION_IN_PROGRESS:
        return core::RetryExecutionResult(
            errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR);

      default:
        return core::FailureExecutionResult(
            errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR);
    }
  }
};
}  // namespace google::scp::core::nosql_database_provider
