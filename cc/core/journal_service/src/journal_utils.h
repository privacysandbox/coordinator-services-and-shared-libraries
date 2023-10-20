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

#pragma once

#include <algorithm>
#include <memory>
#include <string>

#include "core/interface/type_def.h"
#include "core/journal_service/src/error_codes.h"
#include "public/core/interface/execution_result.h"

static constexpr char kCheckpointBlobNamePrefix[] = "checkpoint_";
static constexpr size_t kCheckpointBlobNamePrefixLength = 11;

static constexpr char kJournalBlobNamePrefix[] = "journal_";
static constexpr size_t kJournalBlobNamePrefixLength = 8;

static constexpr size_t kZerosInSuffix = 20;

namespace google::scp::core::journal_service {

class JournalUtils {
 public:
  /**
   * @brief Creates checkpoint blob name.
   *
   * @param partition_name The partition name to create the blob name.
   * @param checkpoint_id The checkpoint id to create the blob name.
   * @param checkpoint_blob_name The output blob name.
   * @return ExecutionResult The execution result of the operation.
   */
  static ExecutionResult CreateCheckpointBlobName(
      const std::shared_ptr<std::string>& partition_name,
      const CheckpointId& checkpoint_id,
      std::shared_ptr<std::string>& checkpoint_blob_name) noexcept {
    return CreateBlobNameWithSuffixId(partition_name, kCheckpointBlobNamePrefix,
                                      checkpoint_id, checkpoint_blob_name);
  }

  /**
   * @brief Creates journal blob name.
   *
   * @param partition_name The partition name to create the blob name.
   * @param journal_id The journal id to create the blob name.
   * @param journal_blob_name The output blob name.
   * @return ExecutionResult The execution result of the operation.
   */
  static ExecutionResult CreateJournalBlobName(
      const std::shared_ptr<std::string>& partition_name,
      const JournalId& journal_id,
      std::shared_ptr<std::string>& journal_blob_name) noexcept {
    return CreateBlobNameWithSuffixId(partition_name, kJournalBlobNamePrefix,
                                      journal_id, journal_blob_name);
  }

  /**
   * @brief Creates a blob with suffix id.
   *
   * @param partition_name The partition name to create the blob name.
   * @param prefix The prefix to be added to a blob name.
   * @param suffix_id The suffix id to be appended to a blob name.
   * @param blob_name The output blob name.
   * @return ExecutionResult The execution result of the operation.
   */
  static ExecutionResult CreateBlobNameWithSuffixId(
      const std::shared_ptr<std::string>& partition_name, const char* prefix,
      const JournalId& journal_suffix_id,
      std::shared_ptr<std::string>& blob_name) noexcept {
    if (!partition_name || prefix == NULL) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME);
    }

    auto suffix_str = std::to_string(journal_suffix_id);
    auto suffix_with_zero_prefix =
        std::string(
            kZerosInSuffix - std::min(kZerosInSuffix, suffix_str.length()),
            '0') +
        suffix_str;

    blob_name = std::make_shared<std::string>(
        *partition_name + std::string("/") + std::string(prefix));
    blob_name->append(suffix_with_zero_prefix);
    return SuccessExecutionResult();
  }

  /**
   * @brief Extracts checkpoint id from a blob name string.
   *
   * @param partition_name The partition name that blob belongs to.
   * @param checkpoint_blob The checkpoint blob name.
   * @param checkpoint_id The extracted checkpoint id.
   * @return ExecutionResult The execution result of the operation.
   */
  static ExecutionResult ExtractCheckpointId(
      const std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<std::string>& checkpoint_blob,
      CheckpointId& checkpoint_id) noexcept {
    return ExtractBlobNameId(partition_name, checkpoint_blob,
                             kCheckpointBlobNamePrefix,
                             kCheckpointBlobNamePrefixLength, checkpoint_id);
  }

  /**
   * @brief Extracts journal id from a blob name string.
   *
   * @param partition_name The partition name that blob belongs to.
   * @param journal_blob The journal blob name.
   * @param journal_id The extracted journal id.
   * @return ExecutionResult The execution result of the operation.
   */
  static ExecutionResult ExtractJournalId(
      const std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<std::string>& journal_blob,
      JournalId& journal_id) noexcept {
    return ExtractBlobNameId(partition_name, journal_blob,
                             kJournalBlobNamePrefix,
                             kJournalBlobNamePrefixLength, journal_id);
  }

  /**
   * @brief Extracts id from a blob name.
   *
   * @param partition_name The partition name that blob belongs to.
   * @param blob_name The blob name to extract the id from.
   * @param prefix The actual prefix of the blob name.
   * @param prefix_length The prefix length.
   * @param extracted_id The extracted id.
   * @return ExecutionResult The execution result of the operation.
   */
  static ExecutionResult ExtractBlobNameId(
      const std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<std::string>& blob_name, const char* prefix,
      size_t prefix_length, JournalId& extracted_id) noexcept {
    if (!partition_name || !blob_name || !prefix) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME);
    }

    std::string full_partition_name = *partition_name + "/";
    if (blob_name->find(full_partition_name) != 0) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME);
    }

    auto blob_name_without_partition_name =
        blob_name->substr(full_partition_name.length());

    // The blob name format of checkpoint blob is journal_[id]
    if (blob_name_without_partition_name.size() <= prefix_length) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME);
    }

    if (blob_name_without_partition_name.find(prefix, 0, prefix_length) != 0) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME);
    }

    try {
      auto start = blob_name_without_partition_name.data() + prefix_length;
      // Skip all the zeros
      while (*start == '0' && *start != '\0') {
        start++;
      }
      extracted_id = std::strtoul(start, NULL, 0);
      return SuccessExecutionResult();
    } catch (std::exception const&) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME);
    }
  }

  /**
   * @brief Create a full path from partition and blob name.
   *
   * @param partition_name The partition name that blob belongs to.
   * @param name The name of the object.
   * @param full_path The full path to be constructed.
   * @return std::shared_ptr<std::string> The constructed blob name.
   */
  static ExecutionResult GetBlobFullPath(
      const std::shared_ptr<std::string>& partition_name,
      const std::shared_ptr<std::string>& name,
      std::shared_ptr<std::string>& full_path) noexcept {
    if (!partition_name || !name) {
      return FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME);
    }
    full_path = std::make_shared<std::string>(*partition_name +
                                              std::string("/") + *name);
    return SuccessExecutionResult();
  }
};
}  // namespace google::scp::core::journal_service
