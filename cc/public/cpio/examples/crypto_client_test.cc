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

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/crypto_client/crypto_client_interface.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/interface/type_def.h"

using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeAead;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::CryptoClientFactory;
using google::scp::cpio::CryptoClientInterface;
using google::scp::cpio::CryptoClientOptions;
using google::scp::cpio::LogOption;
using std::atomic;
using std::bind;
using std::make_unique;
using std::move;
using std::string;
using std::unique_ptr;
using std::placeholders::_1;
using std::placeholders::_2;

constexpr char kPublicKey[] = "testpublickey==";
constexpr char kPrivateKey[] = "testprivatekey=";
constexpr char kSharedInfo[] = "shared_info";
constexpr char kRequestPayload[] = "abcdefg";
constexpr char kResponsePayload[] = "hijklmn";

unique_ptr<CryptoClientInterface> crypto_client;

void AeadDecryptCallback(atomic<bool>& finished, ExecutionResult result,
                         AeadDecryptResponse aead_decrypt_response) {
  finished = true;
  if (result.Successful()) {
    std::cout << "Aead decrypt success! Decrytped response payload: "
              << aead_decrypt_response.payload() << std::endl;
  } else {
    std::cout << "Aead decrypt failure!" << GetErrorMessage(result.status_code)
              << std::endl;
  }
}

void AeadEncryptCallback(atomic<bool>& finished, string& secret,
                         ExecutionResult result,
                         AeadEncryptResponse aead_encrypt_response) {
  if (result.Successful()) {
    std::cout << "Aead encrypt success!" << std::endl;
    AeadDecryptRequest aead_decrypt_request;
    aead_decrypt_request.set_shared_info(string(kSharedInfo));
    aead_decrypt_request.set_secret(secret);
    aead_decrypt_request.mutable_encrypted_data()->set_ciphertext(
        aead_encrypt_response.encrypted_data().ciphertext());
    crypto_client->AeadDecrypt(
        move(aead_decrypt_request),
        bind(AeadDecryptCallback, std::ref(finished), _1, _2));
  } else {
    finished = true;
    std::cout << "Aead encrypt failure!" << GetErrorMessage(result.status_code)
              << std::endl;
  }
}

void HpkeDecryptCallback(bool is_bidirectional, atomic<bool>& finished,
                         ExecutionResult result,
                         HpkeDecryptResponse hpke_decrypt_response) {
  if (result.Successful()) {
    std::cout << "Hpke decrypt success! Decrypted request Payload: "
              << hpke_decrypt_response.payload() << std::endl;
    if (is_bidirectional) {
      std::cout << "Response payload to be encrypted using Aead: "
                << kResponsePayload << std::endl;
      AeadEncryptRequest aead_encrypt_request;
      aead_encrypt_request.set_shared_info(string(kSharedInfo));
      aead_encrypt_request.set_payload(string(kResponsePayload));
      auto secret = hpke_decrypt_response.secret();
      aead_encrypt_request.set_secret(secret);
      crypto_client->AeadEncrypt(
          move(aead_encrypt_request),
          bind(AeadEncryptCallback, std::ref(finished), secret, _1, _2));
    } else {
      finished = true;
    }
  } else {
    finished = true;
    std::cout << "Hpke decrypt failure! " << GetErrorMessage(result.status_code)
              << std::endl;
  }
}

void HpkeEncryptCallback(bool is_bidirectional, atomic<bool>& finished,
                         ExecutionResult result,
                         HpkeEncryptResponse hpke_encrypt_response) {
  if (result.Successful()) {
    std::cout << "Hpke encrypt success!" << std::endl;
    HpkeDecryptRequest hpke_decrypt_request;
    hpke_decrypt_request.mutable_private_key()->set_private_key(kPrivateKey);
    hpke_decrypt_request.set_shared_info(string(kSharedInfo));
    hpke_decrypt_request.set_is_bidirectional(is_bidirectional);
    hpke_decrypt_request.mutable_encrypted_data()->set_ciphertext(
        hpke_encrypt_response.encrypted_data().ciphertext());
    hpke_decrypt_request.mutable_encrypted_data()->set_key_id(
        hpke_encrypt_response.encrypted_data().key_id());
    crypto_client->HpkeDecrypt(move(hpke_decrypt_request),
                               bind(HpkeDecryptCallback, is_bidirectional,
                                    std::ref(finished), _1, _2));
  } else {
    std::cout << "Hpke encrypt failure!" << GetErrorMessage(result.status_code)
              << std::endl;
  }
}

int main(int argc, char* argv[]) {
  bool is_bidirectional = false;
  if (argc > 1) {
    is_bidirectional = string(argv[1]) == "true";
  }

  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  CryptoClientOptions crypto_client_options;

  crypto_client = CryptoClientFactory::Create(move(crypto_client_options));
  result = crypto_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init crypto client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = crypto_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run crypto client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }

  std::cout << "Run crypto client successfully!" << std::endl;

  atomic<bool> finished = false;
  HpkeEncryptRequest hpke_encrypt_request;
  hpke_encrypt_request.mutable_public_key()->set_public_key(string(kPublicKey));
  hpke_encrypt_request.set_shared_info(string(kSharedInfo));
  hpke_encrypt_request.set_payload(string(kRequestPayload));
  hpke_encrypt_request.set_is_bidirectional(is_bidirectional);
  crypto_client->HpkeEncrypt(
      move(hpke_encrypt_request),
      bind(HpkeEncryptCallback, is_bidirectional, std::ref(finished), _1, _2));
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::milliseconds(3000));

  result = crypto_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop crypto client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  return 0;
}
