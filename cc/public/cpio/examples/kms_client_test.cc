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

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "absl/strings/strip.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/kms_client/kms_client_interface.h"
#include "public/cpio/interface/kms_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/kms_service/v1/kms_service.pb.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::KmsClientFactory;
using google::scp::cpio::KmsClientInterface;
using google::scp::cpio::KmsClientOptions;
using google::scp::cpio::LogOption;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::stod;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::chrono::milliseconds;

constexpr char kCiphertext[] = "test_ciphertext";
constexpr char kKeyResourceName[] = "test_key_resource_name";

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  KmsClientOptions kms_client_options;

  auto kms_client = KmsClientFactory::Create(move(kms_client_options));
  result = kms_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init kms client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = kms_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run kms client!" << GetErrorMessage(result.status_code)
              << std::endl;
    return 0;
  }

  std::cout << "Run kms client successfully!" << std::endl;

  auto request = std::make_shared<DecryptRequest>();
  request->set_ciphertext(kCiphertext);
  request->set_key_resource_name(kKeyResourceName);

  atomic<bool> finished = false;
  AsyncContext<DecryptRequest, DecryptResponse> decrypt_context(
      move(request), [&finished](auto& context) {
        if (!context.result.Successful()) {
          std::cout << "Decrypt failed: "
                    << GetErrorMessage(context.result.status_code) << std::endl;
        } else {
          std::cout << "Decrypt succeeded." << std::endl;
        }
        finished = true;
      });
  result = kms_client->Decrypt(decrypt_context);
  if (!result.Successful()) {
    std::cout << "Decrypt failed immediately: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::milliseconds(100000));

  result = kms_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop kms client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
