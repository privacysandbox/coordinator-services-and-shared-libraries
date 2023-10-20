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
#include <variant>

#include <nlohmann/json.hpp>

#include "cpio/client_providers/nosql_database_client_provider/src/common/error_codes.h"
#include "google/cloud/spanner/value.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Provides utility functions for GCP Spanner request flows. GCP uses
 * custom types that need to be converted to SCP types during runtime.
 */
class GcpSpannerUtils {
 public:
  static std::string ConvertAttributeTypeToSpannerTypeName(
      const cmrt::sdk::nosql_database_service::v1::ItemAttribute& attribute) {
    switch (attribute.value_case()) {
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueInt:
        return "INT64";
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueFloat:
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueDouble:
        return "FLOAT64";
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueString:
        return "STRING(MAX)";
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::VALUE_NOT_SET:
      default:
        // Check the validity of this field before calling.
        return "INVALID_TYPE";
    }
  }

  /**
   * @brief Converts Spanner types to ItemAttribute. ItemAttribute.name must be
   * set by caller.
   *
   * @param attribute_value The value of the attribute to be converted.
   * @return core::ExecutionResult<ItemAttribute> The attribute after
   * conversion.
   */
  static core::ExecutionResultOr<
      cmrt::sdk::nosql_database_service::v1::ItemAttribute>
  ConvertJsonTypeToItemAttribute(const nlohmann::json& json_value) {
    cmrt::sdk::nosql_database_service::v1::ItemAttribute attribute;
    // Only primitive - non-nested types are supported.
    if (json_value.is_number_integer()) {
      attribute.set_value_int(json_value.get<int>());
    } else if (json_value.is_number_float()) {
      attribute.set_value_double(json_value.get<double>());
    } else if (json_value.is_string()) {
      attribute.set_value_string(json_value.get<std::string>());
    } else {
      return core::FailureExecutionResult(
          core::errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE);
    }
    return attribute;
  }

  /**
   * @brief Converts NoSQLDatabase attribute types to json type.
   *
   * @param attribute_value The value of the attribute to be converted.
   * @param json_value The json value created from the conversion.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResultOr<nlohmann::json> ConvertItemAttributeToJsonType(
      const cmrt::sdk::nosql_database_service::v1::ItemAttribute& attribute) {
    switch (attribute.value_case()) {
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueInt:
        return attribute.value_int();
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueFloat:
        return attribute.value_float();
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueDouble:
        return attribute.value_double();
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueString:
        return attribute.value_string();
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::VALUE_NOT_SET:
      default:
        return core::FailureExecutionResult(
            core::errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE);
    }
  }

  static core::ExecutionResultOr<google::cloud::spanner::Value>
  ConvertItemAttributeToSpannerValue(
      const cmrt::sdk::nosql_database_service::v1::ItemAttribute& attribute) {
    switch (attribute.value_case()) {
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueInt:
        return google::cloud::spanner::Value(attribute.value_int());
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueFloat:
        // Technically floats are not supported by spanner::Value but it will be
        // converted to a double.
        return google::cloud::spanner::Value(attribute.value_float());
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueDouble:
        return google::cloud::spanner::Value(attribute.value_double());
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::kValueString:
        return google::cloud::spanner::Value(attribute.value_string());
      case cmrt::sdk::nosql_database_service::v1::ItemAttribute::VALUE_NOT_SET:
      default:
        return core::FailureExecutionResult(
            core::errors::SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARAMETER_TYPE);
    }
  }
};
}  // namespace google::scp::cpio::client_providers
