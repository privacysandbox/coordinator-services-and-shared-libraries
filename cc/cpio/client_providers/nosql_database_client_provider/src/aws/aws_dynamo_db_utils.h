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
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>

#include "absl/strings/str_cat.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

namespace google::scp::cpio::client_providers {

// Container for holding the name and AWS value for partition key and optionally
// the sort key.
struct AwsKeyContainer {
  // Name of the partition key.
  Aws::String partition_key_name;
  // Value of the partition key.
  Aws::DynamoDB::Model::AttributeValue partition_key_val;
  // Name of the sort key if present.
  std::optional<Aws::String> sort_key_name;
  // Value of the sort key if present.
  std::optional<Aws::DynamoDB::Model::AttributeValue> sort_key_val;
};

/**
 * @brief Provides utility functions for AWS DynamoDB request flows. Aws uses
 * custom types that need to be converted to SCP types during runtime.
 */
class AwsDynamoDBUtils {
 public:
  /**
   * @brief Creates a ItemAttribute from string object
   * and assigns the value.
   *
   * @tparam T The types to parse the value from the string.
   * @param value The input char array value.
   * @param length The length of the char array.
   * @param no_sql_database_valid_attribute The output object to be p
   * @return ExecutionResult The execution result of the operation.
   */
  template <typename T>
  static core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::ItemAttribute>
  FromString(const char* value, const size_t length) {
    try {
      cmrt::sdk::nosql_database_service::v1::ItemAttribute item_attribute;
      if constexpr (std::is_same<T, int>::value) {
        item_attribute.set_value_int(std::stoi(std::string(value, length)));
        return item_attribute;
      } else if constexpr (std::is_same<T, double>::value) {
        item_attribute.set_value_double(std::stod(std::string(value, length)));
        return item_attribute;
      } else if constexpr (std::is_same<T, float>::value) {
        item_attribute.set_value_float(std::stof(std::string(value, length)));
        return item_attribute;
      } else if constexpr (std::is_same<T, std::string>::value) {
        item_attribute.set_value_string(value, length);
        return item_attribute;
      }
    } catch (...) {}

    return core::FailureExecutionResult(
        core::errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE);
  }

  /**
   * @brief Converts DynamoDB types to ItemAttribute.
   *
   * @param attribute_value The value of the attribute to be converted.
   * @return core::ExecutionResultOr<ItemAttribute> The resultant value
   */
  static core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::ItemAttribute>
  ConvertDynamoDBTypeToItemAttribute(
      const Aws::DynamoDB::Model::AttributeValue& attribute_value) {
    try {
      auto source_type = attribute_value.GetType();
      if (source_type == Aws::DynamoDB::Model::ValueType::NUMBER) {
        const Aws::String& number = attribute_value.GetN();
        // If the number contains a '.', treat as double.
        if (std::any_of(number.begin(), number.end(),
                        [](char c) { return c == '.'; })) {
          auto double_or = FromString<double>(number.c_str(), number.length());
          if (!double_or.Successful()) {
            SCP_ERROR(kDynamoDbUtils, core::common::kZeroUuid,
                      double_or.result(),
                      "Failed converting AWS value to ItemAttribute");
            return double_or.result();
          }
          return *double_or;
        }
        auto int_or = FromString<int>(number.c_str(), number.length());
        if (!int_or.Successful()) {
          SCP_ERROR(kDynamoDbUtils, core::common::kZeroUuid, int_or.result(),
                    "Failed converting AWS value to ItemAttribute");
          return int_or.result();
        }
        return *int_or;
      }
      if (source_type == Aws::DynamoDB::Model::ValueType::STRING) {
        Aws::String str = attribute_value.GetS();
        cmrt::sdk::nosql_database_service::v1::ItemAttribute item_attribute;
        item_attribute.set_value_string(str.c_str());
        return item_attribute;
      }
    } catch (...) {}

    return core::FailureExecutionResult(
        core::errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE);
  }

  /**
   * @brief Converts ItemAttribute to DynamoDB type.
   *
   * @param value The attribute to be converted.
   * @return core::ExecutionResult<Aws::DynamoDB::Model::AttributeValue> The AWS
   * Attribute created from the conversion.
   */
  static core::ExecutionResultOr<Aws::DynamoDB::Model::AttributeValue>
  ConvertItemAttributeToDynamoDBType(
      const cmrt::sdk::nosql_database_service::v1::ItemAttribute& attribute) {
    try {
      Aws::DynamoDB::Model::AttributeValue out_value;
      switch (attribute.value_case()) {
        case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueInt:
          return Aws::DynamoDB::Model::AttributeValue().SetN(
              attribute.value_int());
        case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueFloat:
          return Aws::DynamoDB::Model::AttributeValue().SetN(
              attribute.value_float());
        case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueDouble:
          return Aws::DynamoDB::Model::AttributeValue().SetN(
              attribute.value_double());
        case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueString:
          return Aws::DynamoDB::Model::AttributeValue().SetS(
              attribute.value_string());
        case cmrt::sdk::nosql_database_service::v1::ItemAttribute::
            VALUE_NOT_SET:
        default:
          return core::FailureExecutionResult(
              core::errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE);
      }
    } catch (...) {}

    return core::FailureExecutionResult(
        core::errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE);
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
            core::errors::SC_NO_SQL_DATABASE_PROVIDER_RETRIABLE_ERROR);
      case Aws::DynamoDB::DynamoDBErrors::CONDITIONAL_CHECK_FAILED:
        return core::FailureExecutionResult(
            core::errors::
                SC_NO_SQL_DATABASE_PROVIDER_CONDITIONAL_CHECKED_FAILED);
      default:
        return core::FailureExecutionResult(
            core::errors::SC_NO_SQL_DATABASE_PROVIDER_UNRETRIABLE_ERROR);
    }
  }

  /**
   * @brief Get the partition and sort key names and values from the context
   * object. This is driven by context.request->key().
   *
   * @tparam Context AsyncContext<Request, Response> where Request/Response are
   * either GetDatabaseItem[Request|Response] or
   * UpsertDatabaseItem[Request|Response].
   * @param context The context to process.
   * @return ExecutionResultOr<AwsKeyContainer> The collection of keys parsed
   * from context.
   */
  template <typename Context>
  static core::ExecutionResultOr<AwsKeyContainer> GetPartitionAndSortKeyValues(
      const Context& context) {
    const auto& request = *context.request;
    const auto& request_part_key_name = request.key().partition_key().name();
    AwsKeyContainer key_container;
    key_container.partition_key_name = Aws::String(
        request_part_key_name.c_str(), request_part_key_name.size());
    auto part_key_val_or =
        ConvertItemAttributeToDynamoDBType(request.key().partition_key());
    if (!part_key_val_or.Successful()) {
      SCP_ERROR_CONTEXT(kDynamoDbUtils, context, part_key_val_or.result(),
                        "Error converting partition key type for table %s",
                        request.key().table_name().c_str());
      return part_key_val_or.result();
    }
    key_container.partition_key_val = std::move(*part_key_val_or);

    // Sort key is optional
    if (request.key().has_sort_key()) {
      // Set the sort key
      const auto& request_sort_key_name = request.key().sort_key().name();
      key_container.sort_key_name = Aws::String(request_sort_key_name.c_str(),
                                                request_sort_key_name.size());
      auto sort_key_val_or =
          ConvertItemAttributeToDynamoDBType(request.key().sort_key());
      if (!sort_key_val_or.Successful()) {
        SCP_ERROR_CONTEXT(kDynamoDbUtils, context, sort_key_val_or.result(),
                          "Error converting sort key type for table %s",
                          request.key().table_name().c_str());
        return sort_key_val_or.result();
      }
      key_container.sort_key_val = std::move(*sort_key_val_or);
    }
    return key_container;
  }

  /**
   * @brief Get the condition expression built from
   * context.request->required_attributes() and add values to map
   *
   * @tparam Context AsyncContext<Request, Response> where Request/Response are
   * either GetDatabaseItem[Request|Response] or
   * UpsertDatabaseItem[Request|Response].
   * @param context The context to process.
   * @param attribute_values The map of attribute name to attribute value to
   * pass to AWS.
   * @return ExecutionResultOr<String> The constructed condition expression.
   */
  template <typename Context>
  static core::ExecutionResultOr<Aws::String>
  GetConditionExpressionAndAddValuesToMap(
      const Context& context,
      Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>&
          attribute_values) {
    Aws::String condition_expression;
    condition_expression.reserve(kExpressionsInitialByteSize);

    const auto& request = *context.request;
    for (size_t attribute_index = 0;
         attribute_index < request.required_attributes().size();
         ++attribute_index) {
      condition_expression +=
          absl::StrCat(request.required_attributes(attribute_index).name(),
                       "= :attribute_", attribute_index);

      auto attribute_value_or = ConvertItemAttributeToDynamoDBType(
          request.required_attributes(attribute_index));
      if (!attribute_value_or.Successful()) {
        SCP_ERROR_CONTEXT(kDynamoDbUtils, context, attribute_value_or.result(),
                          "Error converting attribute type for table %s",
                          request.key().table_name().c_str());
        return attribute_value_or.result();
      }

      attribute_values.emplace(absl::StrCat(":attribute_", attribute_index),
                               std::move(*attribute_value_or));

      if (attribute_index + 1 < request.required_attributes_size()) {
        condition_expression += " and ";
      }
    }
    return condition_expression;
  }

 private:
  static constexpr char kDynamoDbUtils[] = "AwsDynamoDBUtils";
  static constexpr size_t kExpressionsInitialByteSize = 1024;
};
}  // namespace google::scp::cpio::client_providers
