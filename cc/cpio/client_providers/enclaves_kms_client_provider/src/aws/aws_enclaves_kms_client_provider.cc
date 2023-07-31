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

#include "aws_enclaves_kms_client_provider.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <utility>

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>

#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/common/src/aws/aws_utils.h"

#include "error_codes.h"

using Aws::Auth::AWSCredentials;
using Aws::Client::ClientConfiguration;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
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
    SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_KMSTOOL_CLI_EXECUTION_FAILED;
using google::scp::core::errors::
    SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::cpio::common::CreateClientConfiguration;
using std::array;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::placeholders::_1;

/// Filename for logging errors
static constexpr char kAwsEnclavesKmsClientProvider[] =
    "AwsEnclavesKmsClientProvider";

static constexpr int kBufferSize = 1024;

static void BuildDecryptCmd(const string& region, const string& ciphertext,
                            const string& access_key_id,
                            const string& access_key_secret,
                            const string& security_token,
                            string& command) noexcept {
  if (!region.empty()) {
    command += string(" --region ") + region;
  }

  if (!access_key_id.empty()) {
    command += string(" --aws-access-key-id ") + access_key_id;
  }

  if (!access_key_secret.empty()) {
    command += string(" --aws-secret-access-key ") + access_key_secret;
  }

  if (!security_token.empty()) {
    command += string(" --aws-session-token ") + security_token;
  }

  if (!ciphertext.empty()) {
    command += string(" --ciphertext ") + ciphertext;
  }

  command = "/kmstool_enclave_cli" + command;
}

namespace google::scp::cpio::client_providers {

ExecutionResult AwsEnclavesKmsClientProvider::Init() noexcept {
  if (!credential_provider_) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_CREDENTIAL_PROVIDER_NOT_FOUND);
    ERROR(kAwsEnclavesKmsClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get credential provider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult AwsEnclavesKmsClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsEnclavesKmsClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsEnclavesKmsClientProvider::Decrypt(
    core::AsyncContext<KmsDecryptRequest, KmsDecryptResponse>&
        decrypt_context) noexcept {
  shared_ptr<string> ciphertext = decrypt_context.request->ciphertext;
  if (!ciphertext || ciphertext->empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
    ERROR(kAwsEnclavesKmsClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get cipher text from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  shared_ptr<string> assume_role_arn =
      decrypt_context.request->account_identity;
  if (!assume_role_arn || assume_role_arn->empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND);
    ERROR(kAwsEnclavesKmsClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get AssumeRole Arn.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return execution_result;
  }

  shared_ptr<string> kms_region = decrypt_context.request->kms_region;
  if (!kms_region || kms_region->empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND);
    ERROR(kAwsEnclavesKmsClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get region.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return execution_result;
  }

  auto get_credentials_request = make_shared<GetRoleCredentialsRequest>();
  get_credentials_request->account_identity = assume_role_arn;
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_session_credentials_context(
          move(get_credentials_request),
          bind(&AwsEnclavesKmsClientProvider::
                   GetSessionCredentialsCallbackToDecrypt,
               this, decrypt_context, _1));
  return credential_provider_->GetRoleCredentials(
      get_session_credentials_context);
}

void AwsEnclavesKmsClientProvider::GetSessionCredentialsCallbackToDecrypt(
    AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& decrypt_context,
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_session_credentials_context) noexcept {
  auto execution_result = get_session_credentials_context.result;
  if (!execution_result.Successful()) {
    ERROR(kAwsEnclavesKmsClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Failed to get AWS Credentials.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  auto get_session_credentials_response =
      *get_session_credentials_context.response;

  string command;
  BuildDecryptCmd(*decrypt_context.request->kms_region,
                  *decrypt_context.request->ciphertext,
                  get_session_credentials_response.access_key_id->c_str(),
                  get_session_credentials_response.access_key_secret->c_str(),
                  get_session_credentials_response.security_token->c_str(),
                  command);

  auto plaintext = make_shared<string>();
  auto execute_result = DecryptUsingEnclavesKmstoolCli(command, *plaintext);

  if (!execute_result.Successful()) {
    decrypt_context.result = execute_result;
    decrypt_context.Finish();
    return;
  }

  auto kms_decrypt_response = make_shared<KmsDecryptResponse>();
  kms_decrypt_response->plaintext = plaintext;
  decrypt_context.response = kms_decrypt_response;
  decrypt_context.result = SuccessExecutionResult();
  decrypt_context.Finish();
}

ExecutionResult AwsEnclavesKmsClientProvider::DecryptUsingEnclavesKmstoolCli(
    const string& command, string& plaintext) noexcept {
  array<char, kBufferSize> buffer;
  string result;
  auto pipe = popen(command.c_str(), "r");
  if (!pipe) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_KMSTOOL_CLI_EXECUTION_FAILED);
    ERROR(kAwsEnclavesKmsClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Enclaves KMSTool Cli execution failed on initializing pipe stream.");
    return execution_result;
  }

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }

  auto return_status = pclose(pipe);
  if (return_status == EXIT_FAILURE) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_ENCLAVES_KMS_CLIENT_PROVIDER_KMSTOOL_CLI_EXECUTION_FAILED);
    ERROR(kAwsEnclavesKmsClientProvider, kZeroUuid, kZeroUuid, execution_result,
          "Enclaves KMSTool Cli execution failed on closing pipe stream.");
    return execution_result;
  }

  plaintext = result;
  return SuccessExecutionResult();
}

std::shared_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider) {
  return make_shared<AwsEnclavesKmsClientProvider>(role_credentials_provider);
}
}  // namespace google::scp::cpio::client_providers
