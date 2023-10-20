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

#include "cpio/client_providers/kms_client_provider/src/aws/tee_aws_kms_client_provider.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "core/utils/src/error_codes.h"
#include "cpio/client_providers/kms_client_provider/mock/aws/mock_tee_aws_kms_client_provider_with_overrides.h"
#include "cpio/client_providers/kms_client_provider/src/aws/tee_error_codes.h"
#include "cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Utils::ByteBuffer;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_CREDENTIAL_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::mock::MockRoleCredentialsProvider;
using google::scp::cpio::client_providers::mock::
    MockTeeAwsKmsClientProviderWithOverrides;
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
class TeeAwsKmsClientProviderTest : public ::testing::Test {
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
    client_ = make_unique<MockTeeAwsKmsClientProviderWithOverrides>(
        mock_credentials_provider_);
  }

  void TearDown() override { EXPECT_SUCCESS(client_->Stop()); }

  unique_ptr<MockTeeAwsKmsClientProviderWithOverrides> client_;
  shared_ptr<RoleCredentialsProviderInterface> mock_credentials_provider_;
};

TEST_F(TeeAwsKmsClientProviderTest, MissingCredentialsProvider) {
  client_ = make_unique<MockTeeAwsKmsClientProviderWithOverrides>(nullptr);

  EXPECT_THAT(
      client_->Init(),
      ResultIs(FailureExecutionResult(
          SC_TEE_AWS_KMS_CLIENT_PROVIDER_CREDENTIAL_PROVIDER_NOT_FOUND)));
}

TEST_F(TeeAwsKmsClientProviderTest, SuccessToDecrypt) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_account_identity(kAssumeRoleArn);
  kms_decrpyt_request->set_kms_region(kRegion);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  atomic<bool> condition = false;

  string expect_command =
      "/kmstool_enclave_cli --region us-east-1"
      " --aws-access-key-id access_key_id"
      " --aws-secret-access-key access_key_secret"
      " --aws-session-token security_token"
      " --ciphertext ";
  expect_command += kCiphertext;

  std::string encoded_text;
  core::utils::Base64Encode(expect_command, encoded_text);
  client_->returned_plaintext = encoded_text;

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->plaintext(), expect_command);
        condition = true;
      });

  EXPECT_SUCCESS(client_->Decrypt(context));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(TeeAwsKmsClientProviderTest, FailedToDecode) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_account_identity(kAssumeRoleArn);
  kms_decrpyt_request->set_kms_region(kRegion);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  atomic<bool> condition = false;

  client_->returned_plaintext = "invalid";

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH)));
        condition = true;
      });

  EXPECT_SUCCESS(client_->Decrypt(context));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(TeeAwsKmsClientProviderTest, MissingCipherText) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_account_identity(kAssumeRoleArn);
  kms_decrpyt_request->set_kms_region(kRegion);
  atomic<bool> condition = false;

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_TEE_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND)));
        condition = true;
      });
  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_TEE_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND)));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(TeeAwsKmsClientProviderTest, MissingAssumeRoleArn) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_kms_region(kRegion);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  atomic<bool> condition = false;

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_TEE_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND)));
        condition = true;
      });
  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_TEE_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND)));
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(TeeAwsKmsClientProviderTest, MissingRegion) {
  EXPECT_SUCCESS(client_->Init());
  EXPECT_SUCCESS(client_->Run());

  auto kms_decrpyt_request = make_shared<DecryptRequest>();
  kms_decrpyt_request->set_account_identity(kAssumeRoleArn);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  atomic<bool> condition = false;

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND)));
        condition = true;
      });
  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND)));
  WaitUntil([&]() { return condition.load(); });
}
}  // namespace google::scp::cpio::client_providers::test
