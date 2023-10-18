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

#include "gcp_kms_client_provider.h"

#include <memory>
#include <utility>

#include <tink/aead.h>

#include "core/utils/src/base64.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "google/cloud/kms/key_management_client.h"
#include "public/cpio/interface/kms_client/type_def.h"

#include "error_codes.h"
#include "gcp_key_management_service_client.h"
#include "gcp_kms_aead.h"

using crypto::tink::Aead;
using google::cloud::kms::KeyManagementServiceClient;
using google::cloud::kms::MakeKeyManagementServiceConnection;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::utils::Base64Decode;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;

/// Filename for logging errors
static constexpr char kGcpKmsClientProvider[] = "GcpKmsClientProvider";

namespace google::scp::cpio::client_providers {

ExecutionResult GcpKmsClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpKmsClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpKmsClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpKmsClientProvider::Decrypt(
    core::AsyncContext<DecryptRequest, DecryptResponse>&
        decrypt_context) noexcept {
  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  const auto& key_arn = decrypt_context.request->key_resource_name();
  if (key_arn.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Failed to get Key Arn from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  auto aead_or =
      aead_provider_->CreateAead(decrypt_context.request->gcp_wip_provider(),
                                 decrypt_context.request->account_identity(),
                                 decrypt_context.request->key_resource_name());
  if (!aead_or.Successful()) {
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, aead_or.result(),
                      "Failed to get Aead.");
    decrypt_context.result = aead_or.result();
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  string decoded_ciphertext;
  auto execution_result = Base64Decode(ciphertext, decoded_ciphertext);
  if (!execution_result.Successful()) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Failed to decode the ciphertext using base64.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }

  auto decrypt_or =
      (*aead_or)->Decrypt(decoded_ciphertext, "" /*associated_data*/);
  if (!decrypt_or.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Aead Decryption failed with error %s.",
                      decrypt_or.status().ToString().c_str());
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
  }
  decrypt_context.response = make_shared<DecryptResponse>();
  decrypt_context.response->set_plaintext(move(*decrypt_or));
  decrypt_context.result = SuccessExecutionResult();
  decrypt_context.Finish();
  return SuccessExecutionResult();
}

ExecutionResultOr<shared_ptr<Aead>> GcpKmsAeadProvider::CreateAead(
    const string& wip_provider, const string& service_account_to_impersonate,
    const string& key_arn) noexcept {
  auto key_management_service_client = CreateKeyManagementServiceClient(
      wip_provider, service_account_to_impersonate);
  auto gcp_key_management_service_client =
      make_shared<GcpKeyManagementServiceClient>(key_management_service_client);

  auto aead_result =
      GcpKmsAead::New(key_arn, gcp_key_management_service_client);
  if (!aead_result.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
    SCP_ERROR(kGcpKmsClientProvider, kZeroUuid, execution_result,
              "Failed to get Key Arn.");
    return execution_result;
  }
  return move(*aead_result);
}

#ifndef TEST_CPIO
shared_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    const shared_ptr<KmsClientOptions>& options,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  return make_shared<GcpKmsClientProvider>();
}
#endif
}  // namespace google::scp::cpio::client_providers
