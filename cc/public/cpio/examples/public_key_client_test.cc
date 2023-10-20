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

#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "public/cpio/interface/public_key_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::PublicKeyClientFactory;
using google::scp::cpio::PublicKeyClientInterface;
using google::scp::cpio::PublicKeyClientOptions;
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

constexpr char kPublicKeyEndpoint[] = "https://test.publickey.com";

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  PublicKeyClientOptions public_key_client_options;
  public_key_client_options.endpoints.emplace_back(kPublicKeyEndpoint);

  auto public_key_client =
      PublicKeyClientFactory::Create(move(public_key_client_options));
  result = public_key_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init public key client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = public_key_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run public key client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }

  std::cout << "Run public key client successfully!" << std::endl;

  ListPublicKeysRequest request;
  atomic<bool> finished = false;
  result = public_key_client->ListPublicKeys(
      move(request),
      [&](const ExecutionResult result, ListPublicKeysResponse response) {
        if (!result.Successful()) {
          std::cout << "ListPublicKeys failed: "
                    << GetErrorMessage(result.status_code) << std::endl;
        } else {
          std::cout << "ListPublicKeys succeeded. The key count is: "
                    << response.public_keys_size() << std::endl;
        }
        finished = true;
      });
  if (!result.Successful()) {
    std::cout << "ListPublicKeys failed immediately: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::milliseconds(100000));

  result = public_key_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop public key client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
