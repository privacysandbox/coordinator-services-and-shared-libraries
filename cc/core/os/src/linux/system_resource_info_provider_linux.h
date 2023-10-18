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

#pragma once

#include <cstdint>
#include <string>

#include "core/os/src/system_resource_info_provider.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::os::linux {
class SystemResourceInfoProviderLinux : public SystemResourceInfoProvider {
 public:
  /**
   * @brief Get the Available Memory (memory that can be used for allocations
   * without swapping) in KB
   *
   * @return uint64_t The memory value
   */
  core::ExecutionResultOr<uint64_t> GetAvailableMemoryKb() noexcept override;

 protected:
  /**
   * @brief Get the Mem Info File Path
   *
   * @return std::string
   */
  virtual std::string GetMemInfoFilePath() noexcept;

  /**
   * @brief Parse a meminfo file line and read the numeric value.
   *
   * @param meminfo_line A string representing a line in the meminfo file.
   * @return ExecutionResultOr<uint64_t>
   */
  core::ExecutionResultOr<uint64_t> GetMemInfoLineEntryKb(
      std::string meminfo_line) noexcept;
};
}  // namespace google::scp::core::os::linux
