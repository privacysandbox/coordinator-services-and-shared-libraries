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

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "cpio/client_providers/kms_client_provider/interface/gcp/gcp_key_management_service_client_interface.h"
#include "google/cloud/kms/key_management_client.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc GcpKeyManagementServiceClientInterface
 */
class GcpKeyManagementServiceClient
    : public GcpKeyManagementServiceClientInterface {
 public:
  GcpKeyManagementServiceClient(
      const std::shared_ptr<google::cloud::kms::KeyManagementServiceClient>&
          kms_client)
      : kms_client_shared_(std::move(kms_client)) {}

  google::cloud::StatusOr<google::cloud::kms::v1::DecryptResponse> Decrypt(
      const google::cloud::kms::v1::DecryptRequest& request) noexcept override;

 private:
  std::shared_ptr<google::cloud::kms::KeyManagementServiceClient>
      kms_client_shared_;
};
}  // namespace google::scp::cpio::client_providers
