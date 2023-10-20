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

#include "cpio/client_providers/kms_client_provider/src/aws/aws_kms_aead.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <aws/core/Aws.h>

#include "core/interface/type_def.h"
#include "cpio/client_providers/kms_client_provider/mock/aws/mock_kms_client.h"
#include "public/core/interface/execution_result.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::KMS::KMSClient;
using Aws::KMS::Model::DecryptOutcome;
using Aws::KMS::Model::DecryptRequest;
using Aws::KMS::Model::DecryptResult;
using Aws::KMS::Model::EncryptOutcome;
using Aws::KMS::Model::EncryptRequest;
using Aws::KMS::Model::EncryptResult;
using crypto::tink::Aead;
using crypto::tink::util::StatusOr;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::client_providers::mock::MockKMSClient;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;

namespace google::scp::cpio::client_providers::test {
class AwsKmsAeadTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  void SetUp() override {
    key_arn_ = "test";
    kms_client_ = make_shared<MockKMSClient>();
    plain_text_ = "text";
    associated_data_ = "data";
    cipher_text_ = "cipher text";

    // Mocks EncryptRequest.
    EncryptRequest encrypt_request;
    encrypt_request.SetKeyId(key_arn_.c_str());
    Aws::Utils::ByteBuffer plaintext_buffer(
        reinterpret_cast<const unsigned char*>(plain_text_.data()),
        plain_text_.length());
    encrypt_request.SetPlaintext(plaintext_buffer);

    // Mocks success EncryptRequestOutcome.
    EncryptResult encrypt_result;
    encrypt_result.SetKeyId(key_arn_.c_str());
    Aws::Utils::ByteBuffer cipher_text_blob(
        reinterpret_cast<const unsigned char*>(cipher_text_.data()),
        cipher_text_.length());
    encrypt_result.SetCiphertextBlob(cipher_text_blob);
    EncryptOutcome encrypt_outcome(encrypt_result);

    // Mocks DecryptRequest.
    DecryptRequest decrypt_request;
    decrypt_request.SetKeyId(key_arn_.c_str());
    Aws::Utils::ByteBuffer ciphertext_buffer(
        reinterpret_cast<const unsigned char*>(cipher_text_.data()),
        cipher_text_.length());
    decrypt_request.SetCiphertextBlob(ciphertext_buffer);

    // Mocks success DecryptRequestOutcome.
    DecryptResult decrypt_result;
    decrypt_result.SetKeyId(key_arn_.c_str());
    Aws::Utils::ByteBuffer plaintext(
        reinterpret_cast<const unsigned char*>(plain_text_.data()),
        plain_text_.length());
    decrypt_result.SetPlaintext(plaintext);
    DecryptOutcome decrypt_outcome(decrypt_result);

    kms_client_->encrypt_request_mock = encrypt_request;
    kms_client_->encrypt_outcome_mock = encrypt_outcome;
    kms_client_->decrypt_request_mock = decrypt_request;
    kms_client_->decrypt_outcome_mock = decrypt_outcome;
  }

  std::string key_arn_;
  std::string plain_text_;
  std::string associated_data_;
  std::string cipher_text_;

  shared_ptr<MockKMSClient> kms_client_;
};

TEST_F(AwsKmsAeadTest, SuccessToCreateAwsKmsAead) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  ASSERT_TRUE(kms_aead.ok());
}

TEST_F(AwsKmsAeadTest, FailedToCreateAwsKmsAeadWithEmptyKeyArn) {
  StatusOr<std::unique_ptr<Aead>> kms_aead = AwsKmsAead::New("", kms_client_);
  ASSERT_FALSE(kms_aead.ok());
  EXPECT_EQ(absl::StatusCode::kInvalidArgument, kms_aead.status().code());
}

TEST_F(AwsKmsAeadTest, FailedToCreateAwsKmsAeadWithEmptyKMSClient) {
  StatusOr<std::unique_ptr<Aead>> kms_aead = AwsKmsAead::New(key_arn_, nullptr);
  ASSERT_FALSE(kms_aead.ok());
  EXPECT_EQ(absl::StatusCode::kInvalidArgument, kms_aead.status().code());
}

TEST_F(AwsKmsAeadTest, SuccessToEncrypt) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  StatusOr<std::string> actual_cipher_text =
      (*kms_aead)->Encrypt(plain_text_, associated_data_);
  ASSERT_TRUE(actual_cipher_text.ok());
  EXPECT_EQ(cipher_text_, *actual_cipher_text);
}

TEST_F(AwsKmsAeadTest, FailedToEncrypt) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  std::string bad_plain_text = "bad text";
  StatusOr<std::string> actual_cipher_text =
      (*kms_aead)->Encrypt(bad_plain_text, associated_data_);
  ASSERT_FALSE(actual_cipher_text.ok());
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            actual_cipher_text.status().code());
}

TEST_F(AwsKmsAeadTest, SuccessToDecrypt) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  StatusOr<std::string> actual_plain_text =
      (*kms_aead)->Decrypt(cipher_text_, associated_data_);
  ASSERT_TRUE(actual_plain_text.ok());
  EXPECT_EQ(plain_text_, *actual_plain_text);
}

TEST_F(AwsKmsAeadTest, FailedToDecrypt) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  std::string bad_cipher_text = "bad text";
  StatusOr<std::string> actual_plain_text =
      (*kms_aead)->Decrypt(bad_cipher_text, associated_data_);
  ASSERT_FALSE(actual_plain_text.ok());
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            actual_plain_text.status().code());
}

}  // namespace google::scp::cpio::client_providers::test
