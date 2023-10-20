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

#include "gtest/gtest.h"
#include "public/cpio/mock/blob_storage_client/mock_blob_storage_client.h"
#include "public/cpio/mock/crypto_client/mock_crypto_client.h"
#include "public/cpio/mock/instance_client/mock_instance_client.h"
#include "public/cpio/mock/kms_client/mock_kms_client.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"
#include "public/cpio/mock/parameter_client/mock_parameter_client.h"
#include "public/cpio/mock/private_key_client/mock_private_key_client.h"
#include "public/cpio/mock/public_key_client/mock_public_key_client.h"
#include "public/cpio/utils/configuration_fetcher/mock/mock_configuration_fetcher.h"

namespace google::scp::cpio {
TEST(PublicMocksTest, CreateMocks) {
  MockBlobStorageClient blob_storage_client;
  MockCryptoClient crypto_client;
  MockInstanceClient instance_client;
  MockKmsClient kms_client;
  MockMetricClient metric_client;
  MockParameterClient parameter_client;
  MockPrivateKeyClient private_key_client;
  MockPublicKeyClient public_key_client;
  MockConfigurationFetcher configuration_fetcher;
}
}  // namespace google::scp::cpio
