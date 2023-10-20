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

#include "tee_aws_kms_client_provider.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <utility>

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>

#include "core/utils/src/base64.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "public/cpio/interface/kms_client/type_def.h"

#include "tee_aws_kms_client_provider_utils.h"
#include "tee_error_codes.h"

using Aws::Auth::AWSCredentials;
using Aws::Client::ClientConfiguration;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
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
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_KMSTOOL_CLI_EXECUTION_FAILED;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::utils::Base64Decode;
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
static constexpr char kTeeAwsKmsClientProvider[] = "TeeAwsKmsClientProvider";

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

  command = "/kmstool_enclave_cli decrypt" + command;
}

namespace google::scp::cpio::client_providers {

ExecutionResult TeeAwsKmsClientProvider::Init() noexcept {
  if (!credential_provider_) {
    auto execution_result = FailureExecutionResult(
        SC_TEE_AWS_KMS_CLIENT_PROVIDER_CREDENTIAL_PROVIDER_NOT_FOUND);
    SCP_ERROR(kTeeAwsKmsClientProvider, kZeroUuid, execution_result,
              "Failed to get credential provider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult TeeAwsKmsClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TeeAwsKmsClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TeeAwsKmsClientProvider::Decrypt(
    core::AsyncContext<DecryptRequest, DecryptResponse>&
        decrypt_context) noexcept {
  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_TEE_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  const auto& assume_role_arn = decrypt_context.request->account_identity();
  if (assume_role_arn.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_TEE_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND);
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get AssumeRole Arn.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return execution_result;
  }

  const auto& kms_region = decrypt_context.request->kms_region();
  if (kms_region.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND);
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get region.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return execution_result;
  }

  auto get_credentials_request = make_shared<GetRoleCredentialsRequest>();
  get_credentials_request->account_identity =
      make_shared<AccountIdentity>(assume_role_arn);
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_session_credentials_context(
          move(get_credentials_request),
          bind(&TeeAwsKmsClientProvider::GetSessionCredentialsCallbackToDecrypt,
               this, decrypt_context, _1),
          decrypt_context);
  return credential_provider_->GetRoleCredentials(
      get_session_credentials_context);
}

void TeeAwsKmsClientProvider::GetSessionCredentialsCallbackToDecrypt(
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context,
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_session_credentials_context) noexcept {
  auto execution_result = get_session_credentials_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get AWS Credentials.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  const auto& get_session_credentials_response =
      *get_session_credentials_context.response;

  string command;
  BuildDecryptCmd(decrypt_context.request->kms_region(),
                  decrypt_context.request->ciphertext(),
                  get_session_credentials_response.access_key_id->c_str(),
                  get_session_credentials_response.access_key_secret->c_str(),
                  get_session_credentials_response.security_token->c_str(),
                  command);

  string plaintext;
  auto execute_result = DecryptUsingEnclavesKmstoolCli(command, plaintext);

  if (!execute_result.Successful()) {
    decrypt_context.result = execute_result;
    decrypt_context.Finish();
    return;
  }

  // Decode the plaintext.
  string decoded_plaintext;
  execute_result = Base64Decode(plaintext, decoded_plaintext);
  if (!execute_result.Successful()) {
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context, execute_result,
                      "Failed to decode data.");
    decrypt_context.result = execute_result;
    decrypt_context.Finish();
    return;
  }

  auto kms_decrypt_response = make_shared<DecryptResponse>();
  kms_decrypt_response->set_plaintext(move(decoded_plaintext));
  decrypt_context.response = kms_decrypt_response;
  decrypt_context.result = SuccessExecutionResult();
  decrypt_context.Finish();
}

ExecutionResult TeeAwsKmsClientProvider::DecryptUsingEnclavesKmstoolCli(
    const string& command, string& plaintext) noexcept {
  array<char, kBufferSize> buffer;
  string result;
  auto pipe = popen(command.c_str(), "r");
  if (!pipe) {
    auto execution_result = FailureExecutionResult(
        SC_TEE_AWS_KMS_CLIENT_PROVIDER_KMSTOOL_CLI_EXECUTION_FAILED);
    // popen will put the error in errno.
    char buffer_arr[9999];
    char* error_msg = strerror_r(errno, buffer_arr, 9999);
    SCP_ERROR(kTeeAwsKmsClientProvider, kZeroUuid, execution_result,
              "Enclaves KMSTool Cli execution failed on initializing pipe "
              "stream. Command: %s Error message: %s.",
              command.c_str(), error_msg);
    return execution_result;
  }

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }

  auto return_status = pclose(pipe);
  if (return_status == EXIT_FAILURE) {
    auto execution_result = FailureExecutionResult(
        SC_TEE_AWS_KMS_CLIENT_PROVIDER_KMSTOOL_CLI_EXECUTION_FAILED);
    // pclose will put the error in errno.
    char buffer_arr[9999];
    char* error_msg = strerror_r(errno, buffer_arr, 9999);
    SCP_ERROR(kTeeAwsKmsClientProvider, kZeroUuid, execution_result,
              "Enclaves KMSTool Cli execution failed on closing pipe stream. "
              "Command: %s Error message: %s",
              command.c_str(), error_msg);
    return execution_result;
  }

  TeeAwsKmsClientProviderUtils::ExtractPlaintext(result, plaintext);
  return SuccessExecutionResult();
}

#ifndef TEST_CPIO
std::shared_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    const shared_ptr<KmsClientOptions>& options,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<core::AsyncExecutorInterface>&
        io_async_executor) noexcept {
  return make_shared<TeeAwsKmsClientProvider>(role_credentials_provider);
}
#endif
}  // namespace google::scp::cpio::client_providers
