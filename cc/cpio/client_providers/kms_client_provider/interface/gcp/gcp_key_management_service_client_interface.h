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

#include "google/cloud/kms/key_management_client.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Provides GCP KMS client.
 */
class GcpKeyManagementServiceClientInterface {
 public:
  /**
   * @brief Provides decrypt function.
   *
   * @param request decrypt request.
   * @return StatusOr<google::cloud::kms::v1DecryptResponse> decrypt result.
   */
  virtual google::cloud::StatusOr<google::cloud::kms::v1::DecryptResponse>
  Decrypt(const google::cloud::kms::v1::DecryptRequest& request) noexcept = 0;
};
}  // namespace google::scp::cpio::client_providers
