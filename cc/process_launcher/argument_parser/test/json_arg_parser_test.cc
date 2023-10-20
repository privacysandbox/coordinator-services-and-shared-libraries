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

#include "process_launcher/argument_parser/src/json_arg_parser.h"

#include <gtest/gtest.h>

#include "core/test/scp_test_base.h"
#include "process_launcher/argument_parser/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::ARGUMENT_PARSER_INVALID_EXEC_ARG_JSON;
using google::scp::core::errors::ARGUMENT_PARSER_INVALID_JSON;
using google::scp::core::errors::ARGUMENT_PARSER_UNKNOWN_TYPE;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::process_launcher::ExecutableArgument;
using google::scp::process_launcher::JsonArgParser;

namespace google::scp::process_launcher::test {
class JsonArgParserTest : public ScpTestBase {};

TEST_F(JsonArgParserTest,
       GivenAnUnkownTypeSpecificationItShouldReturnErrorWhenParsing) {
  JsonArgParser<std::string> parser;
  std::string parsed_value;

  auto result = parser.Parse("", parsed_value);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(ARGUMENT_PARSER_UNKNOWN_TYPE)));
}

TEST_F(JsonArgParserTest,
       GivenInvalidJsonForExecutableArgumentItShouldReturnErrorWhenParsing) {
  JsonArgParser<ExecutableArgument> parser;
  ExecutableArgument parsed_value;

  auto result = parser.Parse("Invalid JSON", parsed_value);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(ARGUMENT_PARSER_INVALID_JSON)));
}

TEST_F(JsonArgParserTest,
       GivenInvalidPayloadForExecutableArgumentItShouldReturnErrorWhenParsing) {
  JsonArgParser<ExecutableArgument> parser;
  ExecutableArgument parsed_value;

  auto result = parser.Parse("{\"unknown_key\":\"unkown val\"}", parsed_value);

  EXPECT_EQ(result,
            FailureExecutionResult(ARGUMENT_PARSER_INVALID_EXEC_ARG_JSON));
}

TEST_F(JsonArgParserTest,
       GivenAValidExecutableArgumentPayloadShouldSuccessfullyParseWithNoArgs) {
  JsonArgParser<ExecutableArgument> parser;
  ExecutableArgument parsed_value;

  auto result = parser.Parse(
      "{\"executable_name\":\"/full/path/to/executable\"}", parsed_value);

  EXPECT_SUCCESS(result);
  EXPECT_EQ("/full/path/to/executable", parsed_value.executable_name);
  EXPECT_EQ(0, parsed_value.command_line_args.size());
}

TEST_F(JsonArgParserTest,
       GivenAValidExecutableArgumentPayloadShouldSuccessfullyParseWithArgs) {
  JsonArgParser<ExecutableArgument> parser;
  ExecutableArgument parsed_value;

  const char* json_string =
      "{"
      "\"executable_name\":\"/full/path/to/executable2\","
      "\"command_line_args\": [ \"arg1\", \"123\" ]"
      "}";

  auto result = parser.Parse(std::string(json_string), parsed_value);

  EXPECT_SUCCESS(result);
  EXPECT_EQ("/full/path/to/executable2", parsed_value.executable_name);
  EXPECT_EQ(2, parsed_value.command_line_args.size());
  EXPECT_EQ("arg1", parsed_value.command_line_args[0]);
  EXPECT_EQ("123", parsed_value.command_line_args[1]);
  EXPECT_TRUE(parsed_value.restart);
}

TEST_F(JsonArgParserTest, ExecutableArgShouldBuildExecutableVectorWithArgs) {
  ExecutableArgument value;
  value.executable_name = "/some/exe/name";
  value.command_line_args = std::vector<std::string>{"arg1", "arg2", "arg3"};

  std::vector<char*> cstring_vec;
  value.ToExecutableVector(cstring_vec);

  ASSERT_EQ(5, cstring_vec.size());
  EXPECT_EQ(0, strcmp("/some/exe/name", cstring_vec.at(0)))
      << "The executable name should be first in the vector";
  EXPECT_EQ(0, strcmp("arg1", cstring_vec.at(1)));
  EXPECT_EQ(0, strcmp("arg2", cstring_vec.at(2)));
  EXPECT_EQ(0, strcmp("arg3", cstring_vec.at(3)));
  EXPECT_EQ(NULL, cstring_vec.at(4)) << "The last element should be NULL";
}

TEST_F(JsonArgParserTest, ExecutableArgShouldBuildExecutableVectorWithNoArgs) {
  ExecutableArgument value;
  value.executable_name = "/some/exe/name";

  std::vector<char*> cstring_vec;
  value.ToExecutableVector(cstring_vec);

  ASSERT_EQ(2, cstring_vec.size());
  EXPECT_EQ(0, strcmp("/some/exe/name", cstring_vec.at(0)))
      << "The executable name should be first in the vector";
  EXPECT_EQ(NULL, cstring_vec.at(1)) << "The last element should be NULL";
}

TEST_F(JsonArgParserTest, SucceedWithTrueShouldRecoverFailuresFlag) {
  JsonArgParser<ExecutableArgument> parser;
  ExecutableArgument parsed_value;

  const char* json_string =
      "{"
      "\"executable_name\":\"/full/path/to/executable2\","
      "\"command_line_args\": [ \"arg1\", \"123\" ],"
      "\"restart\": true"
      "}";

  auto result = parser.Parse(std::string(json_string), parsed_value);

  EXPECT_SUCCESS(result);
  EXPECT_EQ("/full/path/to/executable2", parsed_value.executable_name);
  EXPECT_EQ(2, parsed_value.command_line_args.size());
  EXPECT_TRUE(parsed_value.restart);
}

TEST_F(JsonArgParserTest, SucceedWithFalseShouldRecoverFailuresFlag) {
  JsonArgParser<ExecutableArgument> parser;
  ExecutableArgument parsed_value;

  const char* json_string =
      "{"
      "\"executable_name\":\"/full/path/to/executable2\","
      "\"restart\": false"
      "}";

  auto result = parser.Parse(std::string(json_string), parsed_value);

  EXPECT_SUCCESS(result);
  EXPECT_EQ("/full/path/to/executable2", parsed_value.executable_name);
  EXPECT_EQ(0, parsed_value.command_line_args.size());
  EXPECT_FALSE(parsed_value.restart);
}
}  // namespace google::scp::process_launcher::test
