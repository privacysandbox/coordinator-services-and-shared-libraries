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

#include "core/config_provider/src/config_provider.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>

#include "core/config_provider/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::ConfigProvider;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;

using std::list;
using std::move;
using std::string;
using std::filesystem::path;

namespace google::scp::core::test {
path GetTestDataDir(std::string relative_path) {
  path test_srcdir_env = std::getenv("TEST_SRCDIR");
  path test_workspace_env = std::getenv("TEST_WORKSPACE");

  return path(test_srcdir_env) / path(test_workspace_env) / move(relative_path);
}

TEST(ConfigProviderTest, GetConfigs) {
  path relative_path =
      "cc/core/config_provider/test/resources/test_config.json";
  path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  config.Init();

  string out_string;
  string expect_string = "10.10.10.20";
  size_t out_int;
  size_t expect_int = 5000;
  bool out_bool;
  bool expect_bool = true;
  list<string> out_string_list;
  list<string> expect_string_list({"1", "2"});
  list<int32_t> out_int_list;
  list<int32_t> expect_int_list({1, 2});
  list<size_t> out_size_list;
  list<size_t> expect_size_list({3, 4});
  list<bool> out_bool_list;
  list<bool> expect_bool_list({true, false});

  config.Get("server-ip", out_string);
  config.Get("server-run", out_bool);
  config.Get("buffer-length", out_int);
  config.Get("string-list", out_string_list);
  config.Get("int-list", out_int_list);
  config.Get("size-list", out_size_list);
  config.Get("bool-list", out_bool_list);

  EXPECT_EQ(out_string, expect_string);
  EXPECT_EQ(out_int, expect_int);
  EXPECT_EQ(out_bool, expect_bool);
  EXPECT_EQ(out_string_list, expect_string_list);
  EXPECT_EQ(out_int_list, expect_int_list);
  EXPECT_EQ(out_size_list, expect_size_list);
  EXPECT_EQ(out_bool_list, expect_bool_list);
}

TEST(ConfigProviderTest, GetConfigsFailed) {
  path relative_path =
      "cc/core/config_provider/test/resources/test_config.json";
  path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  config.Init();

  string out_string;

  EXPECT_SUCCESS(config.Init());

  EXPECT_THAT(config.Get("server-name", out_string),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND)));

  EXPECT_THAT(config.Get("buffer-length", out_string),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(ConfigProviderTest, InitFailed) {
  path relative_path =
      "cc/core/config_provider/test/resources/unknown_config.json";
  path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  EXPECT_THAT(config.Init(),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_CANNOT_PARSE_CONFIG_FILE)));
}

TEST(ConfigProviderTest, ShowErrorInfo) {
  path relative_path =
      "cc/core/config_provider/test/resources/unknown_config.json";
  path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  auto status_code = config.Init().status_code;

  string status_description = errors::GetErrorMessage(status_code);

  EXPECT_EQ(status_description, "Config provider cannot load config file");
}
}  // namespace google::scp::core::test
