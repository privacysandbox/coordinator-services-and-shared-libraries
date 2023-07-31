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

#include "cpio/client_providers/private_key_fetching_client_provider/src/private_key_fetching_client_utils.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_ENCRYPTION_KEY_TYPE_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_EXPIRATION_TIME_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_INVALID_ENCRYPTION_KEY_TYPE;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_MATERIAL_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_PUBLIC_KEYSET_HANDLE_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_RESOURCE_NAME_NOT_FOUND;
using google::scp::cpio::client_providers::KeyData;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using std::make_shared;
using std::shared_ptr;
using std::stoi;
using std::string;
using std::to_string;

namespace google::scp::cpio::client_providers::test {

TEST(PrivateKeyFetchingClientUtilsTest, ParsePrivateKeySuccess) {
  string bytes_str = R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": 1669252790485,
        "expirationTime": 1669943990485,
        "ttlTime": 0,
        "keyData": [
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
                "keyMaterial": "test=test"
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(bytes, response);

  EXPECT_EQ(result, SuccessExecutionResult());
  EXPECT_EQ(*response.resource_name, "encryptionKeys/123456");
  EXPECT_EQ(response.encryption_key_type,
            EncryptionKeyType::kMultiPartyHybridEvenKeysplit);
  EXPECT_EQ(*response.public_keyset_handle, "primaryKeyId");
  EXPECT_EQ(*response.public_key_material, "testtest");
  EXPECT_EQ(response.expiration_time_ms, 1669943990485);
  EXPECT_EQ(*response.key_data[0]->key_encryption_key_uri,
            "aws-kms://arn:aws:kms:us-east-1:1234567:key");
  EXPECT_EQ(*response.key_data[0]->public_key_signature, "");
  EXPECT_EQ(*response.key_data[0]->key_material, "test=test");
  EXPECT_EQ(*response.key_data[1]->key_encryption_key_uri,
            "aws-kms://arn:aws:kms:us-east-1:12345:key");
  EXPECT_EQ(*response.key_data[1]->public_key_signature, "");
  EXPECT_EQ(*response.key_data[1]->key_material, "");
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithInvalidKeyData) {
  string bytes_str = R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": 1669252790485,
        "expirationTime": 1669943990485,
        "ttlTime": 0,
        "keyData": [
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "",
                "keyMaterial": "test=test"
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(bytes, response);

  EXPECT_EQ(
      result,
      FailureExecutionResult(
          SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_MATERIAL_NOT_FOUND));
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithInvalidKeyDataNoKekUri) {
  string bytes_str = R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": 1669252790485,
        "expirationTime": 1669943990485,
        "ttlTime": 0,
        "keyData": [
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
                "keyMaterial": ""
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(bytes, response);

  EXPECT_EQ(
      result,
      FailureExecutionResult(
          SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_MATERIAL_NOT_FOUND));
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithInvalidKeyType) {
  string bytes_str = R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT_WRONG",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": 1669252790485,
        "expirationTime": 1669943990485,
        "ttlTime": 0,
        "keyData": [
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
                "keyMaterial": ""
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(bytes, response);

  auto failure_result = FailureExecutionResult(
      SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_INVALID_ENCRYPTION_KEY_TYPE);
  EXPECT_EQ(result, failure_result);
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithNameNotFound) {
  string bytes_str = R"({
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": 1669252790485,
        "expirationTime": 1669943990485,
        "ttlTime": 0,
        "keyData": [
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
                "keyMaterial": ""
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  BytesBuffer bytes(bytes_str.length());
  bytes.bytes->assign(bytes_str.begin(), bytes_str.end());
  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(bytes, response);

  EXPECT_EQ(
      result,
      FailureExecutionResult(
          SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_RESOURCE_NAME_NOT_FOUND));
}

}  // namespace google::scp::cpio::client_providers::test
