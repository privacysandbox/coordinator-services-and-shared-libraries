// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core/nosql_database_provider/src/aws/aws_dynamo_db_utils.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/nosql_database_provider/src/common/error_codes.h"

using Aws::Map;
using Aws::String;
using Aws::Vector;
using Aws::DynamoDB::DynamoDBErrors;
using Aws::DynamoDB::Model::AttributeValue;
using Aws::DynamoDB::Model::ValueType;
using Aws::Utils::ByteBuffer;
using google::scp::core::FailureExecutionResult;
using google::scp::core::NoSQLDatabaseValidAttributeValueTypes;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::nosql_database_provider::AwsDynamoDBUtils;
using std::get;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::core::test {
TEST(AwsDynamoDBUtilsTests,
     ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueTypeNumber) {
  AttributeValue attribute_value;
  attribute_value.SetN("123");

  NoSQLDatabaseValidAttributeValueTypes out_value;
  EXPECT_EQ(AwsDynamoDBUtils::
                ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueType(
                    attribute_value, out_value),
            SuccessExecutionResult());

  EXPECT_EQ(get<double>(out_value), 123);

  attribute_value.SetN("123.1");
  EXPECT_EQ(AwsDynamoDBUtils::
                ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueType(
                    attribute_value, out_value),
            SuccessExecutionResult());

  EXPECT_EQ(get<double>(out_value), 123.1);
}

TEST(AwsDynamoDBUtilsTests,
     ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueTypeString) {
  AttributeValue attribute_value;
  attribute_value.SetS("hello world!");

  NoSQLDatabaseValidAttributeValueTypes out_value;
  EXPECT_EQ(AwsDynamoDBUtils::
                ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueType(
                    attribute_value, out_value),
            SuccessExecutionResult());

  EXPECT_EQ(get<string>(out_value), "hello world!");
}

void SetValueType(AttributeValue& attribute_value, ValueType type) {
  if (type == ValueType::BYTEBUFFER) {
    ByteBuffer byte_buffer;
    attribute_value.SetB(byte_buffer);
  } else if (type == ValueType::STRING_SET) {
    Vector<String> string_set;
    attribute_value.SetSS(string_set);
  } else if (type == ValueType::NUMBER_SET) {
    Vector<String> number_set;
    attribute_value.SetNS(number_set);
  } else if (type == ValueType::BYTEBUFFER_SET) {
    Vector<ByteBuffer> buffer_set;
    attribute_value.SetBS(buffer_set);
  } else if (type == ValueType::ATTRIBUTE_MAP) {
    Map<String, const shared_ptr<AttributeValue>> attribute_map;
    attribute_value.SetM(attribute_map);
  } else if (type == ValueType::ATTRIBUTE_LIST) {
    const Vector<shared_ptr<AttributeValue>> attribute_list;
    attribute_value.SetL(attribute_list);
  } else if (type == ValueType::NULLVALUE) {
    attribute_value.SetNull(false);
  } else if (type == ValueType::BOOL) {
    attribute_value.SetB(false);
  } else {
    EXPECT_EQ(true, false);
  }
}

TEST(AwsDynamoDBUtilsTests,
     ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueTypeInvalidType) {
  vector<ValueType> invalid_value_types(
      {ValueType::BYTEBUFFER, ValueType::STRING_SET, ValueType::NUMBER_SET,
       ValueType::BYTEBUFFER_SET, ValueType::ATTRIBUTE_MAP,
       ValueType::ATTRIBUTE_LIST, ValueType::BOOL, ValueType::NULLVALUE});

  for (auto value_type : invalid_value_types) {
    AttributeValue attribute_value;
    SetValueType(attribute_value, value_type);
    NoSQLDatabaseValidAttributeValueTypes out_value;
    EXPECT_EQ(AwsDynamoDBUtils::
                  ConvertDynamoDBTypeToNoSQLDatabaseValidAttributeValueType(
                      attribute_value, out_value),
              FailureExecutionResult(
                  errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE));
  }
}

TEST(AwsDynamoDBUtilsTests,
     ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBTypeNumber) {
  AttributeValue attribute_value;
  NoSQLDatabaseValidAttributeValueTypes value = static_cast<int>(1);
  EXPECT_EQ(AwsDynamoDBUtils::
                ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
                    value, attribute_value),
            SuccessExecutionResult());

  EXPECT_EQ(attribute_value.GetN(), "1");

  value = static_cast<double>(1.3);
  EXPECT_EQ(AwsDynamoDBUtils::
                ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
                    value, attribute_value),
            SuccessExecutionResult());

  EXPECT_EQ(attribute_value.GetN(), "1.300000");

  value = static_cast<float>(1.5);
  EXPECT_EQ(AwsDynamoDBUtils::
                ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
                    value, attribute_value),
            SuccessExecutionResult());

  EXPECT_EQ(attribute_value.GetN(), "1.500000");
}

TEST(AwsDynamoDBUtilsTests,
     ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBTypeString) {
  AttributeValue attribute_value;
  NoSQLDatabaseValidAttributeValueTypes value = string("hello world!");
  EXPECT_EQ(AwsDynamoDBUtils::
                ConvertNoSQLDatabaseValidAttributeValueTypeToDynamoDBType(
                    value, attribute_value),
            SuccessExecutionResult());

  EXPECT_EQ(string(attribute_value.GetS().c_str()), get<string>(value));
}

TEST(DynamoDBUtilsTests, ConvertDynamoErrorToExecutionResult) {
  // Retriable error codes.
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::INTERNAL_FAILURE),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::SERVICE_UNAVAILABLE),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::THROTTLING),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::SLOW_DOWN),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::REQUEST_TIMEOUT),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::PROVISIONED_THROUGHPUT_EXCEEDED),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::REQUEST_LIMIT_EXCEEDED),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::RESOURCE_IN_USE),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::TABLE_IN_USE),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));
  EXPECT_EQ(AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
                DynamoDBErrors::TRANSACTION_IN_PROGRESS),
            RetryExecutionResult(errors::SC_NO_SQL_DATABASE_RETRIABLE_ERROR));

  // Unretriable error codes
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INCOMPLETE_SIGNATURE),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INVALID_ACTION),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INVALID_CLIENT_TOKEN_ID),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INVALID_PARAMETER_COMBINATION),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INVALID_QUERY_PARAMETER),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INVALID_PARAMETER_VALUE),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::MISSING_ACTION),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::MISSING_AUTHENTICATION_TOKEN),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::MISSING_PARAMETER),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::OPT_IN_REQUIRED),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::REQUEST_EXPIRED),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::VALIDATION),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::ACCESS_DENIED),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::RESOURCE_NOT_FOUND),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::UNRECOGNIZED_CLIENT),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::MALFORMED_QUERY_STRING),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::REQUEST_TIME_TOO_SKEWED),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INVALID_SIGNATURE),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::SIGNATURE_DOES_NOT_MATCH),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INVALID_ACCESS_KEY_ID),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::NETWORK_CONNECTION),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::UNKNOWN),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::BACKUP_IN_USE),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::BACKUP_NOT_FOUND),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::CONDITIONAL_CHECK_FAILED),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::CONTINUOUS_BACKUPS_UNAVAILABLE),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::GLOBAL_TABLE_ALREADY_EXISTS),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::GLOBAL_TABLE_NOT_FOUND),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::IDEMPOTENT_PARAMETER_MISMATCH),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INDEX_NOT_FOUND),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::INVALID_RESTORE_TIME),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::ITEM_COLLECTION_SIZE_LIMIT_EXCEEDED),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::LIMIT_EXCEEDED),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::POINT_IN_TIME_RECOVERY_UNAVAILABLE),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::REPLICA_ALREADY_EXISTS),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::REPLICA_NOT_FOUND),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::TABLE_ALREADY_EXISTS),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::TABLE_NOT_FOUND),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::TRANSACTION_CANCELED),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
  EXPECT_EQ(
      AwsDynamoDBUtils::ConvertDynamoErrorToExecutionResult(
          DynamoDBErrors::TRANSACTION_CONFLICT),
      FailureExecutionResult(errors::SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR));
}
}  // namespace google::scp::core::test
