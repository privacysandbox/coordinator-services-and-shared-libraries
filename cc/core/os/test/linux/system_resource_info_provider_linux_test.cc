/*
 * Copyright 2023 Google LLC
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

#include "core/os/src/linux/system_resource_info_provider_linux.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using std::string;
using ::testing::AllOf;
using ::testing::Gt;

namespace google::scp::core::os::linux::test {
class SystemResourceInfoProviderLinuxForTests
    : public SystemResourceInfoProviderLinux {
 public:
  explicit SystemResourceInfoProviderLinuxForTests(string mem_info_file_path)
      : mem_info_file_path_(mem_info_file_path) {}

  string GetMemInfoFilePath() noexcept override { return mem_info_file_path_; }

 private:
  string mem_info_file_path_;
};

TEST(SystemResourceInfoProviderLinux, ShouldFailIfMemInfoFileIsInvalid) {
  SystemResourceInfoProviderLinuxForTests mem_info(
      "cc/core/os/test/linux/files/invalid_format_meminfo_file.txt");

  auto result_or = mem_info.GetAvailableMemoryKb();

  EXPECT_FALSE(result_or.result().Successful());
}

TEST(SystemResourceInfoProviderLinux,
     ShouldFailIfExpectedFieldMissingInMemInfoFile) {
  SystemResourceInfoProviderLinuxForTests mem_info(
      "cc/core/os/test/linux/files/missing_available_meminfo_file.txt");

  auto result_or = mem_info.GetAvailableMemoryKb();

  EXPECT_FALSE(result_or.result().Successful());
}

TEST(SystemResourceInfoProviderLinux, ShouldFailIfMemInfoFileDoesNotExist) {
  SystemResourceInfoProviderLinuxForTests mem_info(
      "file/that/does/not/exists.txt");

  auto result_or = mem_info.GetAvailableMemoryKb();

  EXPECT_FALSE(result_or.result().Successful());
}

TEST(SystemResourceInfoProviderLinux, ShouldReadMemInfoIfValidFile) {
  SystemResourceInfoProviderLinuxForTests mem_info(
      "cc/core/os/test/linux/files/valid_meminfo_file.txt");

  auto result_or = mem_info.GetAvailableMemoryKb();

  EXPECT_EQ(*result_or, 7922601);
}

TEST(SystemResourceInfoProviderLinux,
     ShouldReadActualMemInfoFileOnLinuxSystem) {
  SystemResourceInfoProviderLinux mem_info;

  auto result_or = mem_info.GetAvailableMemoryKb();

  ASSERT_THAT(*result_or, Gt(1));
}
}  // namespace google::scp::core::os::linux::test
