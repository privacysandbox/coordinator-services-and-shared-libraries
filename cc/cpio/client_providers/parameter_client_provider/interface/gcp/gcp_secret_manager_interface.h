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

#include "core/interface/service_interface.h"
#include "google/cloud/future.h"
#include "google/cloud/secretmanager/secret_manager_client.h"

namespace google::scp::cpio {
/**
 * @brief Provides GCP secret manager service.
 */
class GcpSecretManagerInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Provides AsyncAccessSecretVersion() function to be used to get the
   * selected version response of a secret.
   *
   * @return google::cloud::secretmanager::v1::AccessSecretVersionResponse the
   * response for a given secret version.
   */
  virtual google::cloud::future<google::cloud::StatusOr<
      google::cloud::secretmanager::v1::AccessSecretVersionResponse>>
  AsyncAccessSecretVersion(
      google::cloud::secretmanager::v1::AccessSecretVersionRequest const&
          request,
      google::cloud::Options opts = {}) noexcept = 0;
};
}  // namespace google::scp::cpio
