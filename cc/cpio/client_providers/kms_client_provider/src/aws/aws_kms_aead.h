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
#include <vector>

#include <aws/kms/KMSClient.h>
#include <tink/aead.h>
#include <tink/util/statusor.h>

namespace google::scp::cpio::client_providers {

// This class is copied from Tink::AwsKmsAead
class AwsKmsAead : public ::crypto::tink::Aead {
 public:
  static crypto::tink::util::StatusOr<std::unique_ptr<Aead>> New(
      absl::string_view key_arn,
      std::shared_ptr<Aws::KMS::KMSClient> aws_client);

  crypto::tink::util::StatusOr<std::string> Encrypt(
      absl::string_view plaintext,
      absl::string_view associated_data) const override;

  crypto::tink::util::StatusOr<std::string> Decrypt(
      absl::string_view ciphertext,
      absl::string_view associated_data) const override;

  virtual ~AwsKmsAead() {}

 private:
  AwsKmsAead(absl::string_view key_arn,
             std::shared_ptr<Aws::KMS::KMSClient> aws_client);
  std::string key_arn_;  // The location of a crypto key in AWS KMS.
  std::shared_ptr<Aws::KMS::KMSClient> aws_client_;
};
}  // namespace google::scp::cpio::client_providers
