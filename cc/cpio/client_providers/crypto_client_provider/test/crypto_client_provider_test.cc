// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpio/client_providers/crypto_client_provider/src/crypto_client_provider.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <tink/hybrid/internal/hpke_context.h>
#include <tink/util/secret_data.h>

#include "absl/strings/escaping.h"
#include "core/interface/async_context.h"
#include "core/test/scp_test_base.h"
#include "core/test/utils/conditional_wait.h"
#include "core/utils/src/base64.h"
#include "core/utils/src/error_codes.h"
#include "cpio/client_providers/crypto_client_provider/src/error_codes.h"
#include "proto/hpke.pb.h"
#include "proto/tink.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

using absl::Base64Escape;
using absl::HexStringToBytes;
using crypto::tink::util::SecretData;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeAead;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeKdf;
using google::cmrt::sdk::crypto_service::v1::HpkeKem;
using google::cmrt::sdk::crypto_service::v1::HpkeParams;
using google::crypto::tink::HpkePrivateKey;
using google::crypto::tink::Keyset;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;
using google::scp::core::test::WaitUntil;
using google::scp::core::utils::Base64Encode;
using std::atomic;
using std::function;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;

namespace google::scp::cpio::client_providers::test {
constexpr char kKeyId[] = "key_id";
constexpr char kSharedInfo[] = "shared_info";
constexpr char kPayload[] = "payload";
constexpr char kSecret128[] = "000102030405060708090a0b0c0d0e0f";
constexpr char kSecret256[] =
    "000102030405060708090a0b0c0d0e0f000102030405060708090a0b0c0d0e0f";
constexpr char kPublicKeyForChacha20[] =
    "4310ee97d88cc1f088a5576c77ab0cf5c3ac797f3d95139c6c84b5429c59662a";
constexpr char kPublicKeyForAes128Gcm[] =
    "3948cfe0ad1ddb695d780e59077195da6c56506b027329794ab02bca80815c4d";
constexpr char kDecryptedPrivateKeyForChacha20[] =
    "8057991eef8f1f1af18f4a9491d16a1ce333f695d4db8e38da75975c4478e0fb";
constexpr char kDecryptedPrivateKeyForAes128Gcm[] =
    "4612c550263fc8ad58375df3f557aac531d26850903e55a9f23f21d8534e8ac8";

class CryptoClientProviderTest : public ScpTestBase {
 protected:
  void SetUp() override {
    auto options = make_shared<CryptoClientOptions>();
    client_ = make_unique<CryptoClientProvider>(options);

    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());
  }

  void TearDown() override { EXPECT_SUCCESS(client_->Stop()); }

  AsyncContext<HpkeEncryptRequest, HpkeEncryptResponse>
  CreateHpkeEncryptContext(bool is_bidirectional,
                           const ExecutionResult& decrypt_private_key_result,
                           const string& exporter_context = "",
                           HpkeParams hpke_params_from_request = HpkeParams(),
                           HpkeParams hpke_params_config = HpkeParams()) {
    auto request = make_shared<HpkeEncryptRequest>();
    *request->mutable_hpke_params() = hpke_params_from_request;
    auto public_key = request->mutable_public_key();
    public_key->set_key_id(kKeyId);
    if (hpke_params_from_request.aead() == HpkeAead::AES_128_GCM ||
        hpke_params_config.aead() == HpkeAead::AES_128_GCM) {
      public_key->set_public_key(
          Base64Escape(HexStringToBytes(kPublicKeyForAes128Gcm)));
    } else {
      public_key->set_public_key(
          Base64Escape(HexStringToBytes(kPublicKeyForChacha20)));
    }
    request->set_shared_info(string(kSharedInfo));
    request->set_payload(string(kPayload));
    request->set_is_bidirectional(is_bidirectional);
    request->set_exporter_context(exporter_context);
    return AsyncContext<HpkeEncryptRequest, HpkeEncryptResponse>(
        move(request),
        [exporter_context, hpke_params_from_request, hpke_params_config,
         decrypt_private_key_result,
         this](AsyncContext<HpkeEncryptRequest, HpkeEncryptResponse>& context) {
          if (!context.request->is_bidirectional()) {
            EXPECT_EQ(context.response->secret(), "");
          }
          EXPECT_EQ(context.response->encrypted_data().key_id(), kKeyId);

          auto ciphertext = context.response->encrypted_data().ciphertext();

          auto decrypt_context = CreateHpkeDecryptContext(
              ciphertext, context.request->is_bidirectional(),
              context.response->secret(), decrypt_private_key_result,
              exporter_context, hpke_params_from_request, hpke_params_config);
          EXPECT_EQ(client_->HpkeDecrypt(decrypt_context),
                    decrypt_private_key_result);
        });
  }

  AsyncContext<HpkeDecryptRequest, HpkeDecryptResponse>
  CreateHpkeDecryptContext(string_view ciphertext, bool is_bidirectional,
                           const string& secret,
                           const ExecutionResult& decrypt_private_key_result,
                           const string& exporter_context,
                           HpkeParams hpke_params_from_request,
                           HpkeParams hpke_params_from_config) {
    auto request = make_shared<HpkeDecryptRequest>();
    HpkePrivateKey hpke_private_key;
    if (hpke_params_from_request.aead() == HpkeAead::AES_128_GCM ||
        hpke_params_from_config.aead() == HpkeAead::AES_128_GCM) {
      hpke_private_key.set_private_key(
          HexStringToBytes(kDecryptedPrivateKeyForAes128Gcm));
    } else {
      hpke_private_key.set_private_key(
          HexStringToBytes(kDecryptedPrivateKeyForChacha20));
    }
    Keyset key;
    key.set_primary_key_id(123);
    key.add_key();
    key.mutable_key(0)->set_key_id(456);
    key.mutable_key(0)->mutable_key_data()->set_value(
        hpke_private_key.SerializeAsString());

    auto private_key = request->mutable_private_key();
    private_key->set_key_id(kKeyId);
    string encoded_private_key;
    if (decrypt_private_key_result.Successful()) {
      Base64Encode(key.SerializeAsString(), encoded_private_key);
      private_key->set_private_key(encoded_private_key);
    } else if (decrypt_private_key_result.status_code ==
               SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH) {
      private_key->set_private_key("invalid");
    } else {
      Base64Encode("invalid", encoded_private_key);
      private_key->set_private_key(encoded_private_key);
    }
    request->set_shared_info(string(kSharedInfo));
    request->set_is_bidirectional(is_bidirectional);
    request->mutable_encrypted_data()->set_ciphertext(string(ciphertext));
    request->mutable_encrypted_data()->set_key_id(string(kKeyId));
    request->set_exporter_context(exporter_context);
    return AsyncContext<HpkeDecryptRequest, HpkeDecryptResponse>(
        move(request),
        [decrypt_private_key_result, secret](
            AsyncContext<HpkeDecryptRequest, HpkeDecryptResponse>& context) {
          if (!decrypt_private_key_result.Successful()) {
            EXPECT_THAT(context.result, ResultIs(decrypt_private_key_result));
            return;
          }
          EXPECT_EQ(context.response->payload(), kPayload);
          EXPECT_EQ(context.response->secret(), secret);
        });
  }

  AsyncContext<AeadEncryptRequest, AeadEncryptResponse>
  CreateAeadEncryptContext(string_view secret) {
    auto request = make_shared<AeadEncryptRequest>();
    request->set_shared_info(string(kSharedInfo));
    request->set_payload(string(kPayload));
    request->set_secret(HexStringToBytes(secret));
    return AsyncContext<AeadEncryptRequest, AeadEncryptResponse>(
        move(request),
        [&](AsyncContext<AeadEncryptRequest, AeadEncryptResponse>& context) {});
  }

  AsyncContext<AeadDecryptRequest, AeadDecryptResponse>
  CreateAeadDecryptContext(string_view secret, string_view ciphertext) {
    auto request = make_shared<AeadDecryptRequest>();
    request->set_shared_info(string(kSharedInfo));
    request->set_secret(HexStringToBytes(secret));
    request->mutable_encrypted_data()->set_ciphertext(string(ciphertext));
    return AsyncContext<AeadDecryptRequest, AeadDecryptResponse>(
        move(request),
        [&](AsyncContext<AeadDecryptRequest, AeadDecryptResponse>& context) {});
  }

  unique_ptr<CryptoClientProvider> client_;
};

TEST_F(CryptoClientProviderTest, HpkeEncryptAndDecryptSuccessForOneDirection) {
  auto encrypt_context = CreateHpkeEncryptContext(false /*is_bidirectional*/,
                                                  SuccessExecutionResult());
  EXPECT_SUCCESS(client_->HpkeEncrypt(encrypt_context));
}

TEST_F(CryptoClientProviderTest,
       HpkeEncryptAndDecryptSuccessForInputHpkeParams) {
  HpkeParams hpke_params_from_request;
  hpke_params_from_request.set_aead(HpkeAead::CHACHA20_POLY1305);
  auto encrypt_context = CreateHpkeEncryptContext(
      false /*is_bidirectional*/, SuccessExecutionResult(),
      "" /*exporter_context*/, hpke_params_from_request);
  EXPECT_SUCCESS(client_->HpkeEncrypt(encrypt_context));
}

TEST_F(CryptoClientProviderTest,
       HpkeEncryptAndDecryptSuccessForConfigHpkeParams) {
  auto options = make_shared<CryptoClientOptions>();
  options->hpke_params.set_kem(HpkeKem::DHKEM_X25519_HKDF_SHA256);
  options->hpke_params.set_kdf(HpkeKdf::HKDF_SHA256);
  options->hpke_params.set_aead(HpkeAead::AES_128_GCM);
  client_ = make_unique<CryptoClientProvider>(options);

  auto encrypt_context = CreateHpkeEncryptContext(
      false /*is_bidirectional*/, SuccessExecutionResult(),
      "" /*exporter_context*/, HpkeParams(), options->hpke_params);
  EXPECT_SUCCESS(client_->HpkeEncrypt(encrypt_context));
}

TEST_F(CryptoClientProviderTest, HpkeEncryptAndDecryptSuccessForTwoDirection) {
  auto encrypt_context = CreateHpkeEncryptContext(true /*is_bidirectional*/,
                                                  SuccessExecutionResult());
  EXPECT_SUCCESS(client_->HpkeEncrypt(encrypt_context));
}

TEST_F(CryptoClientProviderTest, HpkeEncryptAndDecryptWithInputExportContext) {
  string exporter_context = "custom exporter";
  auto encrypt_context = CreateHpkeEncryptContext(
      true /*is_bidirectional*/, SuccessExecutionResult(), exporter_context);
  EXPECT_SUCCESS(client_->HpkeEncrypt(encrypt_context));
}

TEST_F(CryptoClientProviderTest, CannotCreateKeyset) {
  auto encrypt_context = CreateHpkeEncryptContext(
      false /*is_bidirectional*/,
      FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE));
  EXPECT_SUCCESS(client_->HpkeEncrypt(encrypt_context));
}

TEST_F(CryptoClientProviderTest, FailedToDecodePrivateKey) {
  auto encrypt_context = CreateHpkeEncryptContext(
      false /*is_bidirectional*/,
      FailureExecutionResult(SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH));
  EXPECT_SUCCESS(client_->HpkeEncrypt(encrypt_context));
}

TEST_F(CryptoClientProviderTest, AeadEncryptAndDecryptSuccessFor128Secret) {
  auto encrypt_context = CreateAeadEncryptContext(kSecret128);
  EXPECT_SUCCESS(client_->AeadEncrypt(encrypt_context));
  EXPECT_SUCCESS(encrypt_context.result);
  auto ciphertext = encrypt_context.response->encrypted_data().ciphertext();

  auto decrypt_context = CreateAeadDecryptContext(kSecret128, ciphertext);
  EXPECT_SUCCESS(client_->AeadDecrypt(decrypt_context));
  EXPECT_SUCCESS(decrypt_context.result);
  EXPECT_EQ(decrypt_context.response->payload(), kPayload);
}

TEST_F(CryptoClientProviderTest, AeadEncryptAndDecryptSuccessFor256Secret) {
  auto encrypt_context = CreateAeadEncryptContext(kSecret256);
  EXPECT_SUCCESS(client_->AeadEncrypt(encrypt_context));
  EXPECT_SUCCESS(encrypt_context.result);
  auto ciphertext = encrypt_context.response->encrypted_data().ciphertext();

  auto decrypt_context = CreateAeadDecryptContext(kSecret256, ciphertext);
  EXPECT_SUCCESS(client_->AeadDecrypt(decrypt_context));
  EXPECT_SUCCESS(decrypt_context.result);
  EXPECT_EQ(decrypt_context.response->payload(), kPayload);
}

TEST_F(CryptoClientProviderTest, CannotCreateAeadDueToInvalidSecret) {
  SecretData invalid_secret(4, 'x');
  string secret_str(invalid_secret.begin(), invalid_secret.end());
  auto encrypt_context = CreateAeadEncryptContext(secret_str);
  EXPECT_THAT(client_->AeadEncrypt(encrypt_context),
              ResultIs(FailureExecutionResult(
                  SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED)));

  auto decrypt_context = CreateAeadDecryptContext(secret_str, kPayload);
  EXPECT_THAT(client_->AeadDecrypt(decrypt_context),
              ResultIs(FailureExecutionResult(
                  SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED)));
}
}  // namespace google::scp::cpio::client_providers::test
