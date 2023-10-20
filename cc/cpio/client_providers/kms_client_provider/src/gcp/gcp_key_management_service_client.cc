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

#include "gcp_key_management_service_client.h"

#include "google/cloud/kms/key_management_client.h"

using google::cloud::Options;
using google::cloud::StatusOr;
using google::cloud::kms::KeyManagementServiceClient;
using google::cloud::kms::v1::DecryptRequest;
using google::cloud::kms::v1::DecryptResponse;

namespace google::scp::cpio::client_providers {
StatusOr<DecryptResponse> GcpKeyManagementServiceClient::Decrypt(
    const DecryptRequest& request) noexcept {
  KeyManagementServiceClient kms_client(*kms_client_shared_);
  return kms_client.Decrypt(request, Options{});
}
}  // namespace google::scp::cpio::client_providers
