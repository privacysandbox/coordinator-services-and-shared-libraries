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

#include "core/journal_service/src/journal_utils.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::journal_service::JournalUtils;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::core::test {
TEST(JournalUtilsTests, CreateCheckpointBlobName) {
  shared_ptr<string> partition_name;
  shared_ptr<string> blob_name;
  EXPECT_THAT(
      JournalUtils::CreateCheckpointBlobName(partition_name, 0, blob_name),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME)));

  partition_name = make_shared<string>("partition_name");
  EXPECT_SUCCESS(
      JournalUtils::CreateCheckpointBlobName(partition_name, 10000, blob_name));
  EXPECT_EQ(*blob_name, "partition_name/checkpoint_00000000000000010000");
}

TEST(JournalUtilsTests, CreateJournalBlobName) {
  shared_ptr<string> partition_name;
  shared_ptr<string> blob_name;
  EXPECT_THAT(JournalUtils::CreateJournalBlobName(partition_name, 0, blob_name),
              ResultIs(FailureExecutionResult(
                  errors::SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME)));

  partition_name = make_shared<string>("partition_name");
  EXPECT_SUCCESS(
      JournalUtils::CreateJournalBlobName(partition_name, 123456, blob_name));
  EXPECT_EQ(*blob_name, "partition_name/journal_00000000000000123456");
}

TEST(JournalUtilsTests, CreateBlobNameWithSuffixId) {
  shared_ptr<string> partition_name;
  shared_ptr<string> blob_name;
  EXPECT_EQ(JournalUtils::CreateBlobNameWithSuffixId(partition_name, NULL, 0,
                                                     blob_name),
            FailureExecutionResult(
                errors::SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME));

  partition_name = make_shared<string>("partition_name");
  EXPECT_EQ(JournalUtils::CreateBlobNameWithSuffixId(
                partition_name, kJournalBlobNamePrefix, 123456, blob_name),
            SuccessExecutionResult());
  EXPECT_EQ(*blob_name, "partition_name/journal_00000000000000123456");

  partition_name = make_shared<string>("partition_name");
  EXPECT_EQ(JournalUtils::CreateBlobNameWithSuffixId(
                partition_name, kJournalBlobNamePrefix, 123456, blob_name),
            SuccessExecutionResult());
  EXPECT_EQ(*blob_name, "partition_name/journal_00000000000000123456");
}

TEST(JournalUtilsTests, ExtractCheckpointId) {
  shared_ptr<string> partition_name;
  shared_ptr<string> blob_name;
  size_t checkpoint_id;

  EXPECT_EQ(
      JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                        checkpoint_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  partition_name = make_shared<string>("partition_name");
  EXPECT_EQ(
      JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                        checkpoint_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  partition_name = nullptr;
  blob_name = make_shared<string>("blob_name");
  EXPECT_EQ(
      JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                        checkpoint_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  partition_name = make_shared<string>("partition_name");
  blob_name = make_shared<string>("blob_name");
  EXPECT_EQ(
      JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                        checkpoint_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name = make_shared<string>("partition_name/");
  EXPECT_EQ(
      JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                        checkpoint_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name = make_shared<string>("partition_name/dsadas");
  EXPECT_EQ(
      JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                        checkpoint_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name = make_shared<string>("partition_name/dsadas_00000000000000012345");
  EXPECT_EQ(
      JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                        checkpoint_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name =
      make_shared<string>("partition_name/journal_00000000000000012345");
  EXPECT_EQ(
      JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                        checkpoint_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name =
      make_shared<string>("partition_name/checkpoint_00000000000000012345");
  EXPECT_EQ(JournalUtils::ExtractCheckpointId(partition_name, blob_name,
                                              checkpoint_id),
            SuccessExecutionResult());

  EXPECT_EQ(checkpoint_id, 12345);
}

TEST(JournalUtilsTests, ExtractJournalId) {
  shared_ptr<string> partition_name;
  shared_ptr<string> blob_name;
  size_t journal_id;

  EXPECT_THAT(
      JournalUtils::ExtractJournalId(partition_name, blob_name, journal_id),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));

  partition_name = make_shared<string>("partition_name");
  EXPECT_THAT(
      JournalUtils::ExtractJournalId(partition_name, blob_name, journal_id),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));

  partition_name = nullptr;
  blob_name = make_shared<string>("blob_name");
  EXPECT_THAT(
      JournalUtils::ExtractJournalId(partition_name, blob_name, journal_id),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));

  partition_name = make_shared<string>("partition_name");
  blob_name = make_shared<string>("blob_name");
  EXPECT_THAT(
      JournalUtils::ExtractJournalId(partition_name, blob_name, journal_id),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));

  blob_name = make_shared<string>("partition_name/");
  EXPECT_THAT(
      JournalUtils::ExtractJournalId(partition_name, blob_name, journal_id),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));

  blob_name = make_shared<string>("partition_name/dsadas");
  EXPECT_THAT(
      JournalUtils::ExtractJournalId(partition_name, blob_name, journal_id),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));

  blob_name = make_shared<string>("partition_name/dsadas_00000000000000012345");
  EXPECT_THAT(
      JournalUtils::ExtractJournalId(partition_name, blob_name, journal_id),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME)));

  blob_name =
      make_shared<string>("partition_name/journal_00000000000000012345");
  EXPECT_SUCCESS(
      JournalUtils::ExtractJournalId(partition_name, blob_name, journal_id));

  EXPECT_EQ(journal_id, 12345);
}

TEST(JournalUtilsTests, ExtractBlobNameId) {
  shared_ptr<string> partition_name;
  shared_ptr<string> blob_name;
  size_t journal_id;

  EXPECT_EQ(
      JournalUtils::ExtractBlobNameId(partition_name, blob_name, NULL,
                                      kJournalBlobNamePrefixLength, journal_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  partition_name = make_shared<string>("partition_name");
  EXPECT_EQ(
      JournalUtils::ExtractBlobNameId(partition_name, blob_name,
                                      kJournalBlobNamePrefix,
                                      kJournalBlobNamePrefixLength, journal_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  partition_name = nullptr;
  blob_name = make_shared<string>("blob_name");
  EXPECT_EQ(
      JournalUtils::ExtractBlobNameId(partition_name, blob_name,
                                      kJournalBlobNamePrefix,
                                      kJournalBlobNamePrefixLength, journal_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  partition_name = make_shared<string>("partition_name");
  blob_name = make_shared<string>("blob_name");
  EXPECT_EQ(
      JournalUtils::ExtractBlobNameId(partition_name, blob_name, NULL,
                                      kJournalBlobNamePrefixLength, journal_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  partition_name = make_shared<string>("partition_name");
  blob_name = make_shared<string>("blob_name");
  EXPECT_EQ(
      JournalUtils::ExtractBlobNameId(partition_name, blob_name,
                                      kJournalBlobNamePrefix,
                                      kJournalBlobNamePrefixLength, journal_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name = make_shared<string>("partition_name/");
  EXPECT_EQ(
      JournalUtils::ExtractBlobNameId(partition_name, blob_name,
                                      kJournalBlobNamePrefix,
                                      kJournalBlobNamePrefixLength, journal_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name = make_shared<string>("partition_name/dsadas");
  EXPECT_EQ(
      JournalUtils::ExtractBlobNameId(partition_name, blob_name,
                                      kJournalBlobNamePrefix,
                                      kJournalBlobNamePrefixLength, journal_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name = make_shared<string>("partition_name/dsadas_12345");
  EXPECT_EQ(
      JournalUtils::ExtractBlobNameId(partition_name, blob_name,
                                      kJournalBlobNamePrefix,
                                      kJournalBlobNamePrefixLength, journal_id),
      FailureExecutionResult(errors::SC_JOURNAL_SERVICE_INVALID_BLOB_NAME));

  blob_name = make_shared<string>("partition_name/journal_12345");
  EXPECT_EQ(JournalUtils::ExtractBlobNameId(
                partition_name, blob_name, kJournalBlobNamePrefix,
                kJournalBlobNamePrefixLength, journal_id),
            SuccessExecutionResult());

  EXPECT_EQ(journal_id, 12345);
}

TEST(JournalUtilsTests, GetBlobFullPath) {
  shared_ptr<string> partition_name;
  shared_ptr<string> blob_name;
  shared_ptr<string> full_path;

  EXPECT_THAT(
      JournalUtils::GetBlobFullPath(partition_name, blob_name, full_path),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME)));

  partition_name = make_shared<string>("partition_name");
  EXPECT_THAT(
      JournalUtils::GetBlobFullPath(partition_name, blob_name, full_path),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME)));

  partition_name = nullptr;
  blob_name = make_shared<string>("blob_name");
  EXPECT_THAT(
      JournalUtils::GetBlobFullPath(partition_name, blob_name, full_path),
      ResultIs(FailureExecutionResult(
          errors::SC_JOURNAL_SERVICE_CANNOT_CREATE_BLOB_NAME)));

  partition_name = make_shared<string>("partition_name");
  EXPECT_SUCCESS(
      JournalUtils::GetBlobFullPath(partition_name, blob_name, full_path));

  EXPECT_EQ(*full_path, "partition_name/blob_name");
}

}  // namespace google::scp::core::test
