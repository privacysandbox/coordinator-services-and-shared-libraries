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

#include "cc/core/config_provider/src/config_provider.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>  // NOLINT
#include <list>
#include <string>
#include <utility>

#include "cc/core/config_provider/src/error_codes.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs_common {
std::filesystem::path GetTestDataDir(std::string relative_path) {
  std::filesystem::path test_srcdir_env = std::getenv("TEST_SRCDIR");
  std::filesystem::path test_workspace_env = std::getenv("TEST_WORKSPACE");

  return std::filesystem::path(test_srcdir_env) /
         std::filesystem::path(test_workspace_env) / std::move(relative_path);
}

TEST(ConfigProviderTest, GetConfigs) {
  std::filesystem::path relative_path =
      "cc/core/config_provider/test/resources/test_config.json";
  std::filesystem::path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  config.Init();

  std::string out_string;
  std::string expect_string = "10.10.10.20";
  size_t out_int;
  size_t expect_int = 5000;
  bool out_bool;
  bool expect_bool = true;
  std::list<std::string> out_string_list;
  std::list<std::string> expect_string_list({"1", "2"});
  std::list<int32_t> out_int_list;
  std::list<int32_t> expect_int_list({1, 2});
  std::list<size_t> out_size_list;
  std::list<size_t> expect_size_list({3, 4});
  std::list<bool> out_bool_list;
  std::list<bool> expect_bool_list({true, false});

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
  std::filesystem::path relative_path =
      "cc/core/config_provider/test/resources/test_config.json";
  std::filesystem::path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  config.Init();

  std::string out_string;

  EXPECT_SUCCESS(config.Init());

  EXPECT_THAT(
      config.Get("server-name", out_string),
      ResultIs(FailureExecutionResult(SC_CONFIG_PROVIDER_KEY_NOT_FOUND)));

  EXPECT_THAT(
      config.Get("buffer-length", out_string),
      ResultIs(FailureExecutionResult(SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(ConfigProviderTest, InitFailed) {
  std::filesystem::path relative_path =
      "cc/core/config_provider/test/resources/unknown_config.json";
  std::filesystem::path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  EXPECT_THAT(config.Init(), ResultIs(FailureExecutionResult(
                                 SC_CONFIG_PROVIDER_CANNOT_PARSE_CONFIG_FILE)));
}

TEST(ConfigProviderTest, ShowErrorInfo) {
  std::filesystem::path relative_path =
      "cc/core/config_provider/test/resources/unknown_config.json";
  std::filesystem::path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  auto status_code = config.Init().status_code;

  std::string status_description = GetErrorMessage(status_code);

  EXPECT_EQ(status_description, "Config provider cannot load config file");
}
}  // namespace privacy_sandbox::pbs_common
