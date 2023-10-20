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

#include "core/config_provider/src/env_config_provider.h"

#include <gtest/gtest.h>

#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <list>
#include <memory>

#include "core/config_provider/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::EnvConfigProvider;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;

using std::list;
using std::move;
using std::string;
using std::to_string;

namespace google::scp::core::test {

TEST(EnvConfigProviderTest, GetConfigsHappyPath) {
  EnvConfigProvider config;

  config.Init();

  string out_string;
  string expect_string = "10.10.10.20";
  char string_val[] = "key-for-string-value=10.10.10.20";
  putenv(string_val);

  bool out_bool;
  bool expect_bool = true;
  char bool_val[] = "key-for-bool-value=true";
  putenv(bool_val);

  size_t out_size_t;
  size_t expect_size_t = 5000;
  char size_t_val[] = "key-for-sizet-value=5000";
  putenv(size_t_val);

  int32_t out_int32_t;
  int32_t expect_int32_t = 6000;
  char int32_t_val[] = "key-for-int32t-value=6000";
  putenv(int32_t_val);

  list<string> out_string_list;
  list<string> expect_string_list({"1", "2"});
  char string_list_val[] = "key-for-string-list=1,2";
  putenv(string_list_val);

  list<int32_t> out_int32_t_list;
  list<int32_t> expect_int32_t_list({1, 2});
  char int32_t_list[] = "key-for-int32t-list=1,2";
  putenv(int32_t_list);

  list<size_t> out_size_t_list;
  list<size_t> expect_size_t_list({3, 4});
  char size_t_list[] = "key-for-sizet-list=3,4";
  putenv(size_t_list);

  list<bool> out_bool_list;
  list<bool> expect_bool_list({true, false});
  char bool_list[] = "key-for-bool-list=true,false";
  putenv(bool_list);

  // string
  auto ret = config.Get("key-for-string-value", out_string);
  EXPECT_SUCCESS(ret);
  // bool
  config.Get("key-for-bool-value", out_bool);
  EXPECT_SUCCESS(ret);
  // size_t
  config.Get("key-for-sizet-value", out_size_t);
  EXPECT_SUCCESS(ret);
  // int32_t
  config.Get("key-for-int32t-value", out_int32_t);
  EXPECT_SUCCESS(ret);
  // string list
  config.Get("key-for-string-list", out_string_list);
  EXPECT_SUCCESS(ret);
  // int32_t list
  config.Get("key-for-int32t-list", out_int32_t_list);
  EXPECT_SUCCESS(ret);
  // size_t list
  config.Get("key-for-sizet-list", out_size_t_list);
  EXPECT_SUCCESS(ret);
  // bool list
  config.Get("key-for-bool-list", out_bool_list);
  EXPECT_SUCCESS(ret);

  EXPECT_EQ(out_string, expect_string);
  EXPECT_EQ(out_size_t, expect_size_t);
  EXPECT_EQ(out_int32_t, expect_int32_t);
  EXPECT_EQ(out_bool, expect_bool);
  EXPECT_EQ(out_string_list, expect_string_list);
  EXPECT_EQ(out_int32_t_list, expect_int32_t_list);
  EXPECT_EQ(out_size_t_list, expect_size_t_list);
  EXPECT_EQ(out_bool_list, expect_bool_list);
}

TEST(EnvConfigProviderTest, WhenSetToEmtpyValueGetStringValueShouldSucceed) {
  EnvConfigProvider config;
  config.Init();

  char empty_set_val[] = "a-var-thats-empty=";
  putenv(empty_set_val);

  std::string out_string;
  EXPECT_SUCCESS(config.Get("a-var-thats-empty", out_string));
  EXPECT_EQ("", out_string);
}

TEST(EnvConfigProviderTest, WhenSetToEmptyValueGetNonStringValueShouldFail) {
  EnvConfigProvider config;
  config.Init();

  char empty_set_val[] = "another-var-thats-empty=";
  putenv(empty_set_val);

  int32_t out_int32;
  EXPECT_THAT(config.Get("another-var-thats-empty", out_int32),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetStringFailsWhenKeyDoesNotExist) {
  EnvConfigProvider config;
  config.Init();

  std::string out_string;
  EXPECT_THAT(config.Get("non-existing-key", out_string),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND)));
}

TEST(EnvConfigProviderTest, GetInt32TFailsWhenValueIsNotInt32) {
  EnvConfigProvider config;
  config.Init();

  char env_var[] = "non-int32-val=hello";
  putenv(env_var);

  int32_t out_val;
  EXPECT_THAT(config.Get("non-int32-val", out_val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetSizeTFailsWhenValueIsNotSizeT) {
  EnvConfigProvider config;
  config.Init();

  char env_var[] = "non-size-t-val=hello";
  putenv(env_var);

  size_t out_val;
  EXPECT_THAT(config.Get("non-size-t-val", out_val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetBoolFailsWhenValueIsNotBool) {
  EnvConfigProvider config;
  config.Init();

  char env_var[] = "non-bool-val=hello";
  putenv(env_var);

  bool out_val;
  EXPECT_THAT(config.Get("non-bool-val", out_val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetStringListFailsWhenDoesNotExist) {
  EnvConfigProvider config;
  config.Init();

  list<string> out_val;
  EXPECT_THAT(config.Get("non-existing-val", out_val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND)));
}

TEST(EnvConfigProviderTest, GetStringListShouldHandleSingleItem) {
  EnvConfigProvider config;
  config.Init();

  char env_var[] = "single-item-list=1";
  putenv(env_var);

  list<string> out_val;
  auto ret = config.Get("single-item-list", out_val);

  EXPECT_SUCCESS(ret);
  list<string> expected_list({"1"});
  EXPECT_EQ(out_val, expected_list);
}

TEST(EnvConfigProviderTest, GetInt32TListShouldFailWhenNotInt32TList) {
  EnvConfigProvider config;
  config.Init();

  char env_var[] = "not-int32t-list=a,2,c";
  putenv(env_var);

  list<int32_t> out_val;
  auto ret = config.Get("not-int32t-list", out_val);

  EXPECT_THAT(ret, ResultIs(FailureExecutionResult(
                       errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetSizeTListShouldFailWhenNotSizeTList) {
  EnvConfigProvider config;
  config.Init();

  char env_var[] = "not-sizet-list=a,2,c";
  putenv(env_var);

  list<size_t> out_val;
  auto ret = config.Get("not-sizet-list", out_val);

  EXPECT_THAT(ret, ResultIs(FailureExecutionResult(
                       errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetBoolListShouldFailWhenNotBoolList) {
  EnvConfigProvider config;
  config.Init();

  char env_var[] = "not-bool-list=a,true,c";
  putenv(env_var);

  list<bool> out_val;
  auto ret = config.Get("not-bool-list", out_val);

  EXPECT_THAT(ret, ResultIs(FailureExecutionResult(
                       errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}
}  // namespace google::scp::core::test
