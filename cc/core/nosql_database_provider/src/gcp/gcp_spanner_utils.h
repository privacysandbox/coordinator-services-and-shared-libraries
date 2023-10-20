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

#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/common/error_codes.h"
#include "google/cloud/spanner/value.h"

namespace google::scp::core::nosql_database_provider {
/**
 * @brief Provides utility functions for GCP Spanner request flows. GCP uses
 * custom types that need to be converted to SCP types during runtime.
 */
class GcpSpannerUtils {
 public:
  /**
   * @brief Converts Cloud Spanner errors to ExecutionResult.
   *
   * @param cloud_storage_error_code Cloud Spanner error codes.
   * @return core::ExecutionResult The Cloud Spanner error code converted to the
   * execution result.
   */
  static core::ExecutionResult ConvertCloudSpannerErrorToExecutionResult(
      const google::cloud::StatusCode cloud_spanner_error_code) noexcept {
    // TODO: Fix and improve these mappings. See the following sites for
    // additional context and more information:
    // https://grpc.github.io/grpc/cpp/namespacegrpc.html#aff1730578c90160528f6a8d67ef5c43b
    // https://cloud.google.com/apis/design/errors#error_info
    // https://grpc.io/grpc/cpp/classgrpc_1_1_status.html

    // Note: The codes kDeadlineExceeded, kUnavailable, kInternal, and
    // kResourceExhausted are not automatically retried by GCP. For all other
    // codes GCP will automatically retry if left unconfigured. This can be
    // turned off or adjusted via the client's Options
    switch (cloud_spanner_error_code) {
      case google::cloud::StatusCode::kResourceExhausted:
      case google::cloud::StatusCode::kUnavailable:
      case google::cloud::StatusCode::kInternal:
      case google::cloud::StatusCode::kUnknown:
      case google::cloud::StatusCode::kAborted:
      case google::cloud::StatusCode::kFailedPrecondition:
      // TODO: If kAlreadyExists can apply to blobs, then convert to
      // BLOB_PATH_EXISTS
      case google::cloud::StatusCode::kAlreadyExists:
        return core::RetryExecutionResult(
            errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR);

      case google::cloud::StatusCode::kNotFound:
        return core::FailureExecutionResult(
            errors::SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND);

      case google::cloud::StatusCode::kOutOfRange:
        return core::FailureExecutionResult(
            errors::SC_NO_SQL_DATABASE_INVALID_REQUEST);

      case google::cloud::StatusCode::kDataLoss:
      case google::cloud::StatusCode::kInvalidArgument:
      case google::cloud::StatusCode::kUnimplemented:
      case google::cloud::StatusCode::kCancelled:
      case google::cloud::StatusCode::kPermissionDenied:
      case google::cloud::StatusCode::kUnauthenticated:
      case google::cloud::StatusCode::kDeadlineExceeded:
      default:
        return core::FailureExecutionResult(
            errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR);
    }
  }

  /**
   * @brief Converts Spanner types to NoSQLDatabase attribute types.
   *
   * @param attribute_value The value of the attribute to be converted.
   * @param out_value The NoSQLDatabase valid typed value created from the
   * conversion.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult
  ConvertJsonTypeToNoSQLDatabaseValidAttributeValueType(
      const nlohmann::json& json_value,
      NoSQLDatabaseValidAttributeValueTypes& out_value) {
    // Only primitive - non-nested types are supported.
    if (json_value.is_null()) {
      return SuccessExecutionResult();
    } else if (json_value.is_number_integer()) {
      out_value = json_value.get<int>();
    } else if (json_value.is_number_float()) {
      out_value = json_value.get<double>();
    } else if (json_value.is_string()) {
      out_value = json_value.get<std::string>();
    } else {
      return core::FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE);
    }
    return SuccessExecutionResult();
  }

  /**
   * @brief Converts NoSQLDatabase attribute types to json type.
   *
   * @param attribute_value The value of the attribute to be converted.
   * @param json_value The json value created from the conversion.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult
  ConvertNoSQLDatabaseValidAttributeValueTypeToJsonType(
      const NoSQLDatabaseValidAttributeValueTypes& attribute_value,
      nlohmann::json& json_value) {
    if (const int* int_val = std::get_if<int>(&attribute_value)) {
      json_value = *int_val;
    } else if (const float* float_val = std::get_if<float>(&attribute_value)) {
      // Technically floats are not supported by spanner::Value but it will be
      // converted to a double.
      json_value = *float_val;
    } else if (const double* double_val =
                   std::get_if<double>(&attribute_value)) {
      json_value = *double_val;
    } else if (const std::string* string_val =
                   std::get_if<std::string>(&attribute_value)) {
      json_value = *string_val;
    } else {
      return core::FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE);
    }
    return SuccessExecutionResult();
  }

  static core::ExecutionResult
  ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
      const NoSQLDatabaseValidAttributeValueTypes& attribute_value,
      google::cloud::spanner::Value& out_value) {
    if (const int* int_val = std::get_if<int>(&attribute_value)) {
      out_value = google::cloud::spanner::Value(*int_val);
    } else if (const float* float_val = std::get_if<float>(&attribute_value)) {
      // Technically floats are not supported by spanner::Value but it will be
      // converted to a double.
      out_value = google::cloud::spanner::Value(*float_val);
    } else if (const double* double_val =
                   std::get_if<double>(&attribute_value)) {
      out_value = google::cloud::spanner::Value(*double_val);
    } else if (const std::string* string_val =
                   std::get_if<std::string>(&attribute_value)) {
      out_value = google::cloud::spanner::Value(*string_val);
    } else {
      return core::FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE);
    }
    return SuccessExecutionResult();
  }
};
}  // namespace google::scp::core::nosql_database_provider
