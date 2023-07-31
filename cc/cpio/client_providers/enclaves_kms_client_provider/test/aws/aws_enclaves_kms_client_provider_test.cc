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

#include "cpio/client_providers/enclaves_kms_client_provider/src/aws/aws_enclaves_kms_client_provider.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/enclaves_kms_client_provider/mock/aws/mock_aws_enclaves_kms_client_provider_with_overrides.h"
#include "cpio/client_providers/enclaves_kms_client_provider/src/aws/error_codes.h"
#include "cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Utils::ByteBuffer;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_CREDENTIAL_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::
    SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::mock::
    MockAwsEnclavesKmsClientProviderWithOverrides;
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
static constexpr char kCiphertext[] = "ciphertext";
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
    mock_credentials_provider_ = make_shared<MockRoleCredentialsProvider>();
    client_ = make_unique<MockAwsEnclavesKmsClientProviderWithOverrides>(
        mock_credentials_provider_);
  }

  void TearDown() override {
    EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
  }

  unique_ptr<MockAwsEnclavesKmsClientProviderWithOverrides> client_;
  shared_ptr<RoleCredentialsProviderInterface> mock_credentials_provider_;
};

TEST_F(AwsEnclavesKmsClientProviderTest, MissingCredentialsProvider) {
  client_ = make_unique<MockAwsEnclavesKmsClientProviderWithOverrides>(nullptr);

  EXPECT_EQ(client_->Init().status_code,
            SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_CREDENTIAL_PROVIDER_NOT_FOUND);
}

TEST_F(AwsEnclavesKmsClientProviderTest, SuccessToDecrypt) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->account_identity = make_shared<string>(kAssumeRoleArn);
  kms_decrpyt_request->kms_region = make_shared<string>(kRegion);
  kms_decrpyt_request->ciphertext = make_shared<string>(kCiphertext);
  atomic<bool> condition = false;

  string expect_command = string("/kmstool_enclave_cli --region us-east-1") +
                          string(" --aws-access-key-id access_key_id") +
                          string(" --aws-secret-access-key access_key_secret") +
                          string(" --aws-session-token security_token") +
                          string(" --ciphertext ") + kCiphertext;
  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        EXPECT_EQ(context.result, SuccessExecutionResult());
        EXPECT_EQ(*context.response->plaintext, expect_command);
        condition = true;
      });

  EXPECT_EQ(client_->Decrypt(context), SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsEnclavesKmsClientProviderTest, MissingCipherText) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->account_identity = make_shared<string>(kAssumeRoleArn);
  kms_decrpyt_request->kms_region = make_shared<string>(kRegion);
  atomic<bool> condition = false;

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        EXPECT_EQ(context.result.status_code,
                  SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
        condition = true;
      });
  EXPECT_EQ(client_->Decrypt(context).status_code,
            SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsEnclavesKmsClientProviderTest, MissingAssumeRoleArn) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->kms_region = make_shared<string>(kRegion);
  kms_decrpyt_request->ciphertext = make_shared<string>(kCiphertext);
  atomic<bool> condition = false;

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        EXPECT_EQ(context.result.status_code,
                  SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND);
        condition = true;
      });
  EXPECT_EQ(client_->Decrypt(context).status_code,
            SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND);
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(AwsEnclavesKmsClientProviderTest, MissingRegion) {
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());

  auto kms_decrpyt_request = make_shared<KmsDecryptRequest>();
  kms_decrpyt_request->account_identity = make_shared<string>(kAssumeRoleArn);
  kms_decrpyt_request->ciphertext = make_shared<string>(kCiphertext);
  atomic<bool> condition = false;

  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& context) {
        EXPECT_EQ(context.result.status_code,
                  SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND);
        condition = true;
      });
  EXPECT_EQ(client_->Decrypt(context).status_code,
            SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND);
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
