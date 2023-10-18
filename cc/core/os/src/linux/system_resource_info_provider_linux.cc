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

#include "system_resource_info_provider_linux.h"

#include <fstream>
#include <sstream>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"

#include "error_codes.h"

using absl::SimpleAtoi;
using absl::SkipEmpty;
using absl::StrContains;
using absl::StrSplit;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using std::ifstream;
using std::string;
using std::stringstream;
using std::vector;

using google::scp::core::errors::
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_FIND_MEMORY_INFO;
using google::scp::core::errors::
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_OPEN_MEMINFO_FILE;
using google::scp::core::errors::
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_PARSE_MEMINFO_LINE;

static constexpr char kMemInfoFileName[] = "/proc/meminfo";
static constexpr char kMemInfoLineSeparator[] = " ";
static constexpr int kExpectedMemInfoLinePartsCount = 3;
static constexpr int kExpectedMemInfoLineNumericValueIndex = 1;
static constexpr char kTotalAvailableMemory[] = "MemAvailable";

namespace google::scp::core::os::linux {
ExecutionResultOr<uint64_t>
SystemResourceInfoProviderLinux::GetAvailableMemoryKb() noexcept {
  auto meminfo_file_path = GetMemInfoFilePath();
  ifstream meminfo_file(meminfo_file_path);
  if (meminfo_file.fail()) {
    return FailureExecutionResult(
        SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_OPEN_MEMINFO_FILE);
  }
  // The contents of this file look like:
  // MemTotal:       198065040 kB
  // MemAvailable:   185711588 kB
  // ...

  std::string line = "";
  uint64_t total_available_mem_kb = 0;

  while (std::getline(meminfo_file, line)) {
    if (StrContains(line, kTotalAvailableMemory)) {
      auto mem_value = GetMemInfoLineEntryKb(line);
      if (mem_value.Successful()) {
        total_available_mem_kb = *mem_value;
      } else {
        return mem_value.result();
      }
    }
  }

  if (total_available_mem_kb < 1) {
    return FailureExecutionResult(
        SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_FIND_MEMORY_INFO);
  }

  return total_available_mem_kb;
}

std::string SystemResourceInfoProviderLinux::GetMemInfoFilePath() noexcept {
  return kMemInfoFileName;
}

ExecutionResultOr<uint64_t>
SystemResourceInfoProviderLinux::GetMemInfoLineEntryKb(
    string meminfo_line) noexcept {
  vector<string> line_parts =
      StrSplit(meminfo_line, kMemInfoLineSeparator, SkipEmpty());

  if (line_parts.size() != kExpectedMemInfoLinePartsCount) {
    return FailureExecutionResult(
        SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_PARSE_MEMINFO_LINE);
  }

  uint64_t read_memory_kb;
  stringstream int_parsing_stream;
  int_parsing_stream << line_parts.at(kExpectedMemInfoLineNumericValueIndex);
  int_parsing_stream >> read_memory_kb;

  if (!SimpleAtoi(line_parts.at(kExpectedMemInfoLineNumericValueIndex),
                  &read_memory_kb)) {
    return FailureExecutionResult(
        SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_PARSE_MEMINFO_LINE);
  }

  return read_memory_kb;
}
}  // namespace google::scp::core::os::linux
