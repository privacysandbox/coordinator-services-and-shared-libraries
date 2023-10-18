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

#include <memory>

#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/kms/key_management_client.h"

#include "gcp_kms_client_provider.h"
#include "tee_gcp_kms_client_provider_utils.h"

using google::cloud::MakeExternalAccountCredentials;
using google::cloud::UnifiedCredentialsOption;
using google::cloud::kms::KeyManagementServiceClient;
using google::cloud::kms::MakeKeyManagementServiceConnection;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace google::scp::cpio::client_providers {

shared_ptr<KeyManagementServiceClient>
GcpKmsAeadProvider::CreateKeyManagementServiceClient(
    const string& wip_provider,
    const string& service_account_to_impersonate) noexcept {
  string credentials_json;
  TeeGcpKmsClientProviderUtils::CreateAttestedCredentials(
      wip_provider, service_account_to_impersonate, credentials_json);
  auto options = google::cloud::Options{}.set<UnifiedCredentialsOption>(
      MakeExternalAccountCredentials(credentials_json));
  auto service_connection = MakeKeyManagementServiceConnection(options);
  return make_shared<KeyManagementServiceClient>(service_connection);
}
}  // namespace google::scp::cpio::client_providers
