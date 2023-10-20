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

#include "nontee_aws_kms_client_provider.h"

#include <memory>
#include <utility>

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>

#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "public/cpio/interface/kms_client/type_def.h"

#include "aws_kms_aead.h"
#include "nontee_error_codes.h"

using Aws::Auth::AWSCredentials;
using Aws::Client::ClientConfiguration;
using Aws::KMS::KMSClient;
using crypto::tink::Aead;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::utils::Base64Decode;
using google::scp::cpio::common::CreateClientConfiguration;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

/// Filename for logging errors
static constexpr char kNonteeAwsKmsClientProvider[] =
    "NonteeAwsKmsClientProvider";

namespace google::scp::cpio::client_providers {

ExecutionResult NonteeAwsKmsClientProvider::Init() noexcept {
  if (!role_credentials_provider_) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_KMS_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND);
    SCP_ERROR(kNonteeAwsKmsClientProvider, kZeroUuid, execution_result,
              "Failed to get credential provider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult NonteeAwsKmsClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult NonteeAwsKmsClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult NonteeAwsKmsClientProvider::Decrypt(
    core::AsyncContext<DecryptRequest, DecryptResponse>&
        decrypt_context) noexcept {
  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  const auto& key_arn = decrypt_context.request->key_resource_name();
  if (key_arn.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get Key Arn from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  const auto& kms_region = decrypt_context.request->kms_region();
  if (kms_region.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get Key Region from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  const auto& account_identity = decrypt_context.request->account_identity();
  if (account_identity.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kNonteeAwsKmsClientProvider, decrypt_context, execution_result,
        "Failed to get Account Identity from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  AsyncContext<DecryptRequest, Aead> get_aead_context(
      decrypt_context.request,
      bind(&NonteeAwsKmsClientProvider::GetAeadCallbackToDecrypt, this,
           decrypt_context, _1),
      decrypt_context);
  return GetAead(get_aead_context);
}

ExecutionResult NonteeAwsKmsClientProvider::GetAeadCallbackToDecrypt(
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context,
    AsyncContext<DecryptRequest, Aead>& get_aead_context) noexcept {
  auto execution_result = get_aead_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get Aead.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  string decoded_ciphertext;
  Base64Decode(decrypt_context.request->ciphertext(), decoded_ciphertext);

  auto decrypt_result =
      get_aead_context.response->Decrypt(decoded_ciphertext, "");
  if (!decrypt_result.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Aead Decryption failed with error %s.",
                      decrypt_result.status().ToString().c_str());
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }
  decrypt_context.response = make_shared<DecryptResponse>();
  decrypt_context.response->set_plaintext(move(*decrypt_result));
  decrypt_context.result = SuccessExecutionResult();
  decrypt_context.Finish();
  return SuccessExecutionResult();
}

ExecutionResult NonteeAwsKmsClientProvider::GetAead(
    AsyncContext<DecryptRequest, Aead>& get_aead_context) noexcept {
  AsyncContext<DecryptRequest, KMSClient> create_kms_context(
      get_aead_context.request,
      bind(&NonteeAwsKmsClientProvider::CreateKmsCallbackToCreateAead, this,
           get_aead_context, _1),
      get_aead_context);
  return CreateKmsClient(create_kms_context);
}

void NonteeAwsKmsClientProvider::CreateKmsCallbackToCreateAead(
    AsyncContext<DecryptRequest, Aead>& get_aead_context,
    AsyncContext<DecryptRequest, KMSClient>& create_kms_context) noexcept {
  auto execution_result = create_kms_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, get_aead_context,
                      execution_result, "Failed to create KMS Client.");
    get_aead_context.result = execution_result;
    get_aead_context.Finish();
    return;
  }

  auto aead_result =
      AwsKmsAead::New(get_aead_context.request->key_resource_name(),
                      move(create_kms_context.response));
  if (!aead_result.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, get_aead_context,
                      execution_result, "Failed to get Key Arn.");
    get_aead_context.result = execution_result;
    get_aead_context.Finish();
    return;
  }
  get_aead_context.response = move(*aead_result);
  get_aead_context.result = SuccessExecutionResult();
  get_aead_context.Finish();
}

ExecutionResult NonteeAwsKmsClientProvider::CreateKmsClient(
    AsyncContext<DecryptRequest, KMSClient>& create_kms_context) noexcept {
  auto request = make_shared<GetRoleCredentialsRequest>();
  request->account_identity = make_shared<AccountIdentity>(
      create_kms_context.request->account_identity());
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_role_credentials_context(
          move(request),
          bind(&NonteeAwsKmsClientProvider::
                   GetSessionCredentialsCallbackToCreateKms,
               this, create_kms_context, _1),
          create_kms_context);
  return role_credentials_provider_->GetRoleCredentials(
      get_role_credentials_context);
}

void NonteeAwsKmsClientProvider::GetSessionCredentialsCallbackToCreateKms(
    AsyncContext<DecryptRequest, KMSClient>& create_kms_context,
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_session_credentials_context) noexcept {
  auto execution_result = get_session_credentials_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider,
                      get_session_credentials_context, execution_result,
                      "Failed to get AWS Credentials.");
    create_kms_context.result = execution_result;
    create_kms_context.Finish();
    return;
  }

  const GetRoleCredentialsResponse& response =
      *get_session_credentials_context.response;
  auto aws_credentials = make_shared<AWSCredentials>(
      response.access_key_id->c_str(), response.access_key_secret->c_str(),
      response.security_token->c_str());

  auto kms_client = GetKmsClient(
      move(aws_credentials),
      make_shared<string>(create_kms_context.request->kms_region()));
  create_kms_context.response = kms_client;
  create_kms_context.result = SuccessExecutionResult();
  create_kms_context.Finish();
}

shared_ptr<ClientConfiguration>
NonteeAwsKmsClientProvider::CreateClientConfiguration(
    const string& region) noexcept {
  auto client_config =
      common::CreateClientConfiguration(make_shared<string>(region));
  client_config->executor = make_shared<AwsAsyncExecutor>(io_async_executor_);
  return client_config;
}

shared_ptr<KMSClient> NonteeAwsKmsClientProvider::GetKmsClient(
    const shared_ptr<AWSCredentials>& aws_credentials,
    const shared_ptr<string>& kms_region) noexcept {
  return make_shared<KMSClient>(*aws_credentials,
                                *CreateClientConfiguration(*kms_region));
}

#ifndef TEST_CPIO
std::shared_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    const shared_ptr<KmsClientOptions>& options,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<core::AsyncExecutorInterface>&
        io_async_executor) noexcept {
  return make_shared<NonteeAwsKmsClientProvider>(role_credentials_provider,
                                                 io_async_executor);
}
#endif
}  // namespace google::scp::cpio::client_providers
