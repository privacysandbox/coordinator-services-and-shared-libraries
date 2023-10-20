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
#include <vector>

#include <aws/core/utils/Outcome.h>
#include <aws/kms/KMSClient.h>
#include <aws/kms/model/DecryptRequest.h>
#include <aws/kms/model/EncryptRequest.h>

namespace google::scp::cpio::client_providers::mock {
class MockKMSClient : public Aws::KMS::KMSClient {
 public:
  Aws::KMS::Model::EncryptOutcome Encrypt(
      const Aws::KMS::Model::EncryptRequest& request) const override {
    if (request.GetKeyId() == encrypt_request_mock.GetKeyId() &&
        request.GetPlaintext() == encrypt_request_mock.GetPlaintext()) {
      return encrypt_outcome_mock;
    }
    Aws::Client::AWSError<Aws::KMS::KMSErrors> error(
        Aws::KMS::KMSErrors::INTERNAL_FAILURE, false);
    Aws::KMS::Model::EncryptOutcome outcome(error);
    return outcome;
  }

  Aws::KMS::Model::DecryptOutcome Decrypt(
      const Aws::KMS::Model::DecryptRequest& request) const override {
    if (request.GetKeyId() == decrypt_request_mock.GetKeyId() &&
        request.GetCiphertextBlob() ==
            decrypt_request_mock.GetCiphertextBlob()) {
      return decrypt_outcome_mock;
    }
    Aws::Client::AWSError<Aws::KMS::KMSErrors> error(
        Aws::KMS::KMSErrors::INTERNAL_FAILURE, false);
    Aws::KMS::Model::DecryptOutcome outcome(error);
    return outcome;
  }

  Aws::KMS::Model::EncryptRequest encrypt_request_mock;
  Aws::KMS::Model::DecryptRequest decrypt_request_mock;
  Aws::KMS::Model::EncryptOutcome encrypt_outcome_mock;
  Aws::KMS::Model::DecryptOutcome decrypt_outcome_mock;
};
}  // namespace google::scp::cpio::client_providers::mock
