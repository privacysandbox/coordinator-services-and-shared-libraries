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

#include <gmock/gmock.h>

#include "cpio/client_providers/kms_client_provider/interface/gcp/gcp_key_management_service_client_interface.h"

namespace google::scp::cpio::client_providers::mock {
/*! @copydoc GcpKeyManagementServiceClientInterface
 */
class MockGcpKeyManagementServiceClient
    : public GcpKeyManagementServiceClientInterface {
 public:
  MOCK_METHOD(google::cloud::StatusOr<google::cloud::kms::v1::DecryptResponse>,
              Decrypt, ((const google::cloud::kms::v1::DecryptRequest&)),
              (noexcept, override));
};
}  // namespace google::scp::cpio::client_providers::mock
