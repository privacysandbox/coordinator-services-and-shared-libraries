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

#include "cpio/client_providers/kms_client_provider/src/gcp/gcp_kms_aead.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "cpio/client_providers/kms_client_provider/mock/gcp/mock_gcp_key_management_service_client.h"
#include "google/cloud/kms/key_management_client.h"

using crypto::tink::Aead;
using crypto::tink::util::StatusOr;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::kms::v1::DecryptRequest;
using google::cloud::kms::v1::DecryptResponse;
using google::scp::cpio::client_providers::mock::
    MockGcpKeyManagementServiceClient;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::Return;

static constexpr char kKeyName[] = "test";
static constexpr char kPlaintext[] = "plaintext";
static constexpr char kCiphertext[] = "ciphertext";
static constexpr char kAssociatedData[] = "data";

namespace google::scp::cpio::client_providers::test {
class GcpKmsAeadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_gcp_kms_client_ = make_shared<MockGcpKeyManagementServiceClient>();
  }

  shared_ptr<MockGcpKeyManagementServiceClient> mock_gcp_kms_client_;
};

TEST_F(GcpKmsAeadTest, SuccessToCreateGcpKmsAead) {
  StatusOr<unique_ptr<Aead>> kms_aead = GcpKmsAead::New(
      kKeyName, dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
                    mock_gcp_kms_client_));
  ASSERT_TRUE(kms_aead.ok());
}

TEST_F(GcpKmsAeadTest, FailedToCreateGcpKmsAeadWithEmptyKeyArn) {
  StatusOr<unique_ptr<Aead>> kms_aead = GcpKmsAead::New(
      "", dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
              mock_gcp_kms_client_));
  ASSERT_FALSE(kms_aead.ok());
  EXPECT_EQ(absl::StatusCode::kInvalidArgument, kms_aead.status().code());
}

TEST_F(GcpKmsAeadTest, FailedToCreateGcpKmsAeadWithEmptyKMSClient) {
  StatusOr<unique_ptr<Aead>> kms_aead = GcpKmsAead::New(kKeyName, nullptr);
  ASSERT_FALSE(kms_aead.ok());
  EXPECT_EQ(absl::StatusCode::kInvalidArgument, kms_aead.status().code());
}

MATCHER_P(RequestMatches, req, "") {
  return ExplainMatchResult(Eq(req.name()), arg.name(), result_listener) &&
         ExplainMatchResult(Eq(req.ciphertext()), arg.ciphertext(),
                            result_listener) &&
         ExplainMatchResult(Eq(req.additional_authenticated_data()),
                            arg.additional_authenticated_data(),
                            result_listener);
}

TEST_F(GcpKmsAeadTest, SuccessToDecrypt) {
  DecryptRequest decrypt_request;
  decrypt_request.set_name(kKeyName);
  decrypt_request.set_ciphertext(string(kCiphertext));
  decrypt_request.set_additional_authenticated_data(string(kAssociatedData));
  DecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  EXPECT_CALL(*mock_gcp_kms_client_, Decrypt(RequestMatches(decrypt_request)))
      .WillOnce(Return(decrypt_response));

  StatusOr<unique_ptr<Aead>> kms_aead = GcpKmsAead::New(
      kKeyName, dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
                    mock_gcp_kms_client_));
  StatusOr<string> actual_plain_text =
      (*kms_aead)->Decrypt(kCiphertext, kAssociatedData);

  ASSERT_TRUE(actual_plain_text.ok());
  EXPECT_EQ(kPlaintext, *actual_plain_text);
}

TEST_F(GcpKmsAeadTest, FailedToDecrypt) {
  DecryptRequest req;
  req.set_name(kKeyName);
  req.set_ciphertext(string(kCiphertext));
  req.set_additional_authenticated_data(string(kAssociatedData));
  EXPECT_CALL(*mock_gcp_kms_client_, Decrypt(RequestMatches(req)))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "Invalid input")));

  StatusOr<unique_ptr<Aead>> kms_aead = GcpKmsAead::New(
      kKeyName, dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
                    mock_gcp_kms_client_));
  StatusOr<string> actual_plain_text =
      (*kms_aead)->Decrypt(kCiphertext, kAssociatedData);

  ASSERT_FALSE(actual_plain_text.ok());
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            actual_plain_text.status().code());
}
}  // namespace google::scp::cpio::client_providers::test
