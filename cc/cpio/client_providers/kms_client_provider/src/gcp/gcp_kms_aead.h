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

#include <tink/aead.h>
#include <tink/util/statusor.h>

#include "absl/strings/string_view.h"
#include "google/cloud/kms/key_management_client.h"

#include "gcp_key_management_service_client.h"

namespace google::scp::cpio::client_providers {

// GcpKmsAead is an implementation of AEAD that forwards
// encryption/decryption requests to a key managed by
// <a href="https://cloud.google.com/kms/">Google Cloud KMS</a>.
class GcpKmsAead : public ::crypto::tink::Aead {
 public:
  // Creates a new GcpKmsAead that is bound to the key specified in 'key_name'.
  // Valid values for 'key_name' have the following format:
  //    projects/*/locations/*/keyRings/*/cryptoKeys/*.
  // See https://cloud.google.com/kms/docs/object-hierarchy for more info.
  static crypto::tink::util::StatusOr<std::unique_ptr<::crypto::tink::Aead>>
  New(absl::string_view key_name,
      std::shared_ptr<GcpKeyManagementServiceClientInterface> kms_client);

  crypto::tink::util::StatusOr<std::string> Encrypt(
      absl::string_view plaintext,
      absl::string_view associated_data) const override;

  crypto::tink::util::StatusOr<std::string> Decrypt(
      absl::string_view ciphertext,
      absl::string_view associated_data) const override;

  virtual ~GcpKmsAead() = default;

 private:
  GcpKmsAead(
      absl::string_view key_name,
      std::shared_ptr<GcpKeyManagementServiceClientInterface> kms_client);
  // The location of a crypto key in GCP KMS.
  std::string key_name_;
  std::shared_ptr<GcpKeyManagementServiceClientInterface> kms_client_;
};

}  // namespace google::scp::cpio::client_providers
