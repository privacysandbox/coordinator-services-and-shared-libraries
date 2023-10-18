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

#include "core/nosql_database_provider/src/gcp/gcp_spanner_utils.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/nosql_database_provider/src/common/error_codes.h"
#include "google/cloud/spanner/value.h"

using google::cloud::spanner::Value;
using google::scp::core::NoSQLDatabaseValidAttributeValueTypes;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::nosql_database_provider::GcpSpannerUtils;
using json = nlohmann::json;
using std::get;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::core::test {
TEST(GcpSpannerUtilsTest,
     ConvertJsonTypeToNoSQLDatabaseValidAttributeValueTypeNumber) {
  json json_val(123);

  NoSQLDatabaseValidAttributeValueTypes out_value;
  EXPECT_EQ(
      GcpSpannerUtils::ConvertJsonTypeToNoSQLDatabaseValidAttributeValueType(
          json_val, out_value),
      SuccessExecutionResult());

  EXPECT_EQ(get<int>(out_value), 123);

  json_val = 123.1f;
  EXPECT_EQ(
      GcpSpannerUtils::ConvertJsonTypeToNoSQLDatabaseValidAttributeValueType(
          json_val, out_value),
      SuccessExecutionResult());

  EXPECT_EQ(get<double>(out_value), 123.1f);
}

TEST(GcpSpannerUtilsTest,
     ConvertJsonTypeToNoSQLDatabaseValidAttributeValueTypeString) {
  json json_val("hello world!");

  NoSQLDatabaseValidAttributeValueTypes out_value;
  EXPECT_EQ(
      GcpSpannerUtils::ConvertJsonTypeToNoSQLDatabaseValidAttributeValueType(
          json_val, out_value),
      SuccessExecutionResult());

  EXPECT_EQ(get<string>(out_value), "hello world!");
}

TEST(GcpSpannerUtilsTest,
     ConvertJsonTypeToNoSQLDatabaseValidAttributeValueTypeInvalidType) {
  vector<json> invalid_value_types(
      {/*struct*/ json::object_t{{"struct_field", 1}},
       /*array*/ vector<string>{"item_1", "item_2"}, /*bool*/ true});

  for (const auto& value_type : invalid_value_types) {
    NoSQLDatabaseValidAttributeValueTypes out_value;
    EXPECT_EQ(
        GcpSpannerUtils::ConvertJsonTypeToNoSQLDatabaseValidAttributeValueType(
            value_type, out_value),
        FailureExecutionResult(
            errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE));
  }
}

TEST(GcpSpannerUtilsTest,
     ConvertNoSQLDatabaseValidAttributeValueTypeToJsonTypeNumber) {
  NoSQLDatabaseValidAttributeValueTypes value_type = 1;
  json json_value;
  EXPECT_EQ(
      GcpSpannerUtils::ConvertNoSQLDatabaseValidAttributeValueTypeToJsonType(
          value_type, json_value),
      SuccessExecutionResult());

  ASSERT_TRUE(json_value.is_number_integer());

  EXPECT_EQ(json_value.get<int>(), 1);

  value_type = static_cast<float>(1.5f);
  EXPECT_EQ(
      GcpSpannerUtils::ConvertNoSQLDatabaseValidAttributeValueTypeToJsonType(
          value_type, json_value),
      SuccessExecutionResult());

  ASSERT_TRUE(json_value.is_number_float());

  EXPECT_EQ(json_value.get<float>(), 1.5f);

  value_type = static_cast<double>(1.5f);
  EXPECT_EQ(
      GcpSpannerUtils::ConvertNoSQLDatabaseValidAttributeValueTypeToJsonType(
          value_type, json_value),
      SuccessExecutionResult());

  ASSERT_TRUE(json_value.is_number_float());

  EXPECT_EQ(json_value.get<double>(), 1.5f);
}

TEST(GcpSpannerUtilsTest,
     ConvertNoSQLDatabaseValidAttributeValueTypeToJsonTypeString) {
  NoSQLDatabaseValidAttributeValueTypes value_type = "hello world!";
  json json_val;
  EXPECT_EQ(
      GcpSpannerUtils::ConvertNoSQLDatabaseValidAttributeValueTypeToJsonType(
          value_type, json_val),
      SuccessExecutionResult());

  ASSERT_TRUE(json_val.is_string());

  EXPECT_EQ(json_val.get<string>(), "hello world!");
}

TEST(GcpSpannerUtilsTest,
     ConvertNoSQLDatabaseAttributeValueTypeToSpannerValueNumber) {
  NoSQLDatabaseValidAttributeValueTypes value_type = 1;
  Value spanner_val;
  EXPECT_EQ(
      GcpSpannerUtils::ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
          value_type, spanner_val),
      SuccessExecutionResult());

  auto int_val_or = spanner_val.get<int64_t>();
  ASSERT_TRUE(int_val_or.ok()) << int_val_or.status().message();
  EXPECT_EQ(*int_val_or, 1);

  value_type = static_cast<float>(1.5f);
  EXPECT_EQ(
      GcpSpannerUtils::ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
          value_type, spanner_val),
      SuccessExecutionResult());

  // Note that we intentionally cast it to a double instead of a float so that
  // Because float is not a Spanner supported type, but will cast floats to
  // doubles.
  auto float_val_or = spanner_val.get<double>();
  ASSERT_TRUE(float_val_or.ok()) << float_val_or.status().message();
  EXPECT_EQ(*float_val_or, 1.5f);

  value_type = static_cast<double>(1.5f);
  EXPECT_EQ(
      GcpSpannerUtils::ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
          value_type, spanner_val),
      SuccessExecutionResult());

  auto double_val_or = spanner_val.get<double>();
  ASSERT_TRUE(double_val_or.ok()) << double_val_or.status().message();
  EXPECT_EQ(*double_val_or, 1.5f);
}

TEST(GcpSpannerUtilsTest,
     ConvertNoSQLDatabaseAttributeValueTypeToSpannerValueString) {
  NoSQLDatabaseValidAttributeValueTypes value_type = "hello world!";
  Value spanner_val;
  EXPECT_EQ(
      GcpSpannerUtils::ConvertNoSQLDatabaseAttributeValueTypeToSpannerValue(
          value_type, spanner_val),
      SuccessExecutionResult());

  auto string_val_or = spanner_val.get<string>();
  ASSERT_TRUE(string_val_or.ok()) << string_val_or.status().message();
  EXPECT_EQ(*string_val_or, "hello world!");
}
}  // namespace google::scp::core::test
