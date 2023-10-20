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

#include "cpio/client_providers/kms_client_provider/src/aws/tee_aws_kms_client_provider_utils.h"

#include <gtest/gtest.h>

#include <string>

using std::string;

namespace google::scp::cpio::client_providers::test {
TEST(TeeAwsKmsClientProviderUtilsTest, ExtractPlaintext) {
  string plaintext;
  string decrypt_result = "answdetset";
  TeeAwsKmsClientProviderUtils::ExtractPlaintext(decrypt_result, plaintext);
  EXPECT_EQ(plaintext, decrypt_result);

  string decrypt_result_1 = "PLAINTEXT: " + decrypt_result;
  TeeAwsKmsClientProviderUtils::ExtractPlaintext(decrypt_result_1, plaintext);
  EXPECT_EQ(plaintext, decrypt_result);

  string decrypt_result_2 = "PLAINTEXT: " + decrypt_result + "\n";
  TeeAwsKmsClientProviderUtils::ExtractPlaintext(decrypt_result_2, plaintext);
  EXPECT_EQ(plaintext, decrypt_result);

  string decrypt_result_3 = decrypt_result + "\n";
  TeeAwsKmsClientProviderUtils::ExtractPlaintext(decrypt_result_3, plaintext);
  EXPECT_EQ(plaintext, decrypt_result);
}
}  // namespace google::scp::cpio::client_providers::test
