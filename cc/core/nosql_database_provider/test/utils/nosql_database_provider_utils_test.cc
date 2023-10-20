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

#include "core/nosql_database_provider/src/common/nosql_database_provider_utils.h"

#include <gtest/gtest.h>

#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::nosql_database_provider::NoSQLDatabaseProviderUtils;
using google::scp::core::test::ResultIs;
using std::get;
using std::string;

namespace google::scp::core::test {

TEST(NoSQLDatabaseProviderUtilsTest, FromStringInt) {
  string int_str("1");
  NoSQLDatabaseValidAttributeValueTypes value;
  EXPECT_EQ(NoSQLDatabaseProviderUtils::FromString<int>(
                int_str.c_str(), int_str.length(), value),
            SuccessExecutionResult());

  EXPECT_EQ(get<int>(value), 1);
}

TEST(NoSQLDatabaseProviderUtilsTest, FromStringDouble) {
  string double_str("1.0");
  NoSQLDatabaseValidAttributeValueTypes value;
  EXPECT_EQ(NoSQLDatabaseProviderUtils::FromString<double>(
                double_str.c_str(), double_str.length(), value),
            SuccessExecutionResult());

  EXPECT_EQ(get<double>(value), 1.0);
}

TEST(NoSQLDatabaseProviderUtilsTest, FromStringFloat) {
  string float_str("1.0");
  NoSQLDatabaseValidAttributeValueTypes value;
  EXPECT_EQ(NoSQLDatabaseProviderUtils::FromString<float>(
                float_str.c_str(), float_str.length(), value),
            SuccessExecutionResult());

  EXPECT_EQ(get<float>(value), 1.0);
}

TEST(NoSQLDatabaseProviderUtilsTest, FromStringString) {
  string str("1.0");
  NoSQLDatabaseValidAttributeValueTypes value;
  EXPECT_EQ(NoSQLDatabaseProviderUtils::FromString<string>(str.c_str(),
                                                           str.length(), value),
            SuccessExecutionResult());

  EXPECT_EQ(get<string>(value), "1.0");
}

TEST(NoSQLDatabaseProviderUtilsTest, FromStringInvalid) {
  string str("s");
  NoSQLDatabaseValidAttributeValueTypes value;
  EXPECT_THAT(NoSQLDatabaseProviderUtils::FromString<string>(NULL, 123, value),
              ResultIs(FailureExecutionResult(
                  errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE)));

  EXPECT_THAT(
      NoSQLDatabaseProviderUtils::FromString<int>(str.c_str(), 123, value),
      ResultIs(FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE)));

  EXPECT_THAT(
      NoSQLDatabaseProviderUtils::FromString<double>(str.c_str(), 123, value),
      ResultIs(FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE)));

  EXPECT_THAT(
      NoSQLDatabaseProviderUtils::FromString<float>(str.c_str(), 123, value),
      ResultIs(FailureExecutionResult(
          errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE)));
}

}  // namespace google::scp::core::test
