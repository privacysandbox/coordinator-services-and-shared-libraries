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

#include "cpio/client_providers/kms_client_provider/src/aws/aws_kms_client_provider.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/kms/KMSClient.h>
#include <aws/kms/KMSErrors.h>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/kms_client_provider/mock/aws/mock_aws_kms_client_provider_with_overrides.h"
#include "cpio/client_providers/kms_client_provider/src/aws/error_codes.h"
#include "cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Client::AWSError;
using Aws::KMS::KMSClient;
using Aws::KMS::KMSErrors;
using Aws::KMS::Model::DecryptOutcome;
using Aws::KMS::Model::DecryptRequest;
using Aws::KMS::Model::DecryptResult;
using Aws::Utils::ByteBuffer;
using crypto::tink::Aead;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::utils::Base64Decode;

using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::
    MockAwsKmsClientProviderWithOverrides;
using google::scp::cpio::client_providers::mock::MockKMSClient;
using google::scp::cpio::client_providers::mock::MockRoleCredentialsProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

static constexpr char kAssumeRoleArn[] = "assumeRoleArn";
static constexpr char kKeyArnWithPrefix[] = "aws-kms://keyArn";
static constexpr char kKeyArn[] = "keyArn";
static constexpr char kWrongKeyArn[] = "aws-kms://wrongkeyArn";
static constexpr char kCiphertext[] = "ciphertext";
static constexpr char kPlaintext[] = "plaintext";
static constexpr char kRegion[] = "us-east-1";

namespace google::scp::cpio::client_providers::test {
class AwsEnclavesKmsClientProviderTest : public ::testing::Test {
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
    mock_kms_client_ = make_shared<MockKMSClient>();

    // Mocks DecryptRequest.
    DecryptRequest decrypt_request;
    decrypt_request.SetKeyId(kKeyArn);
    string ciphertext = string(kCiphertext);
    string decoded_ciphertext;
    Base64Decode(ciphertext, decoded_ciphertext);
    ByteBuffer ciphertext_buffer(
        reinterpret_cast<const unsigned char*>(decoded_ciphertext.data()),
        decoded_ciphertext.length());
    decrypt_request.SetCiphertextBlob(ciphertext_buffer);
    mock_kms_client_->decrypt_request_mock = decrypt_request;

    // Mocks success DecryptRequestOutcome.
    DecryptResult decrypt_result;
    decrypt_result.SetKeyId(kKeyArn);
    string plaintext = string(kPlaintext);
    ByteBuffer plaintext_buffer(
        reinterpret_cast<const unsigned char*>(plaintext.data()),
        plaintext.length());
    decrypt_result.SetPlaintext(plaintext_buffer);
    DecryptOutcome decrypt_outcome(decrypt_result);
    mock_kms_client_->decrypt_outcome_mock = decrypt_outcome;

    mock_credentials_provider_ = make_shared<MockRoleCredentialsProvider>();
    client_ = make_unique<MockAwsKmsClientProviderWithOverrides>(
        mock_credentials_provider_, mock_kms_client_);
  }

  void TearDown() override {
    EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
  }

  unique_ptr<MockAwsKmsClientProviderWithOverrides> client_;
  shared_ptr<MockKMSClient> mock_kms_client_;
  shared_ptr<RoleCredentialsProviderInterface> mock_credentials_provider_;
};

TEST_F(AwsEnclavesKmsClientProviderTest, MissingCredentialsProvider) {
  client_ = make_unique<MockAwsKmsClientProviderWithOverrides>(
      nullptr, mock_kms_client_);

  EXPECT_EQ(client_->Init().status_code,
            SC_AWS_KMS_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND);
}

TEST_F(AwsEnclavesKmsClientProviderTest, MissingAssumeRoleArn) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->kms_region = make_shared<string>(kRegion);
  kms_decrpyt_request->key_arn = make_shared<string>(kKeyArnWithPrefix);
  kms_decrpyt_request->ciphertext = make_shared<string>(kCiphertext);

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {});

  EXPECT_EQ(
      client_->Decrypt(context),
      FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND));
}

TEST_F(AwsEnclavesKmsClientProviderTest, MissingRegion) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->account_identity = make_shared<string>(kAssumeRoleArn);
  kms_decrpyt_request->key_arn = make_shared<string>(kKeyArnWithPrefix);
  kms_decrpyt_request->ciphertext = make_shared<string>(kCiphertext);

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {});

  EXPECT_EQ(
      client_->Decrypt(context),

      FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND));
}

TEST_F(AwsEnclavesKmsClientProviderTest, SuccessToDecrypt) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->kms_region = make_shared<string>(kRegion);
  kms_decrpyt_request->account_identity = make_shared<string>(kAssumeRoleArn);
  kms_decrpyt_request->key_arn = make_shared<string>(kKeyArnWithPrefix);
  kms_decrpyt_request->ciphertext = make_shared<string>(kCiphertext);
  atomic<bool> condition = false;

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        EXPECT_EQ(context.result, SuccessExecutionResult());
        EXPECT_EQ(*context.response->plaintext.get(), kPlaintext);
        condition = true;
      });

  EXPECT_EQ(client_->Decrypt(context), SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsEnclavesKmsClientProviderTest, MissingCipherText) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->kms_region = make_shared<string>(kRegion);
  kms_decrpyt_request->account_identity = make_shared<string>(kAssumeRoleArn);
  kms_decrpyt_request->key_arn = make_shared<string>(kKeyArnWithPrefix);
  atomic<bool> condition = false;

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        EXPECT_EQ(context.result.status_code,
                  SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
        condition = true;
      });
  EXPECT_EQ(client_->Decrypt(context).status_code,
            SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsEnclavesKmsClientProviderTest, MissingKeyArn) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->kms_region = make_shared<string>(kRegion);
  kms_decrpyt_request->account_identity = make_shared<string>(kAssumeRoleArn);
  kms_decrpyt_request->ciphertext = make_shared<string>(kCiphertext);
  atomic<bool> condition = false;

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        condition = true;
        EXPECT_EQ(context.result.status_code,
                  SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND);
      });
  EXPECT_EQ(client_->Decrypt(context).status_code,
            SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsEnclavesKmsClientProviderTest, FailedDecryption) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->kms_region = make_shared<string>(kRegion);
  kms_decrpyt_request->account_identity = make_shared<string>(kAssumeRoleArn);
  kms_decrpyt_request->key_arn = make_shared<string>(kWrongKeyArn);
  kms_decrpyt_request->ciphertext = make_shared<string>(kCiphertext);
  atomic<bool> condition = false;

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        condition = true;
        EXPECT_EQ(context.result.status_code,
                  SC_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED);
      });
  EXPECT_EQ(client_->Decrypt(context), SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
