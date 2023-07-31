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

#include <aws/core/Aws.h>

#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/config_client/config_client_interface.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/test/global_cpio/test_lib_cpio.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::ConfigClientFactory;
using google::scp::cpio::ConfigClientInterface;
using google::scp::cpio::ConfigClientOptions;
using google::scp::cpio::GetInstanceIdRequest;
using google::scp::cpio::GetInstanceIdResponse;
using google::scp::cpio::GetTagRequest;
using google::scp::cpio::GetTagResponse;
using google::scp::cpio::LogOption;
using google::scp::cpio::TestCpioOptions;
using google::scp::cpio::TestLibCpio;
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

static constexpr char kRegion[] = "us-east-1";
static constexpr char kInstanceId[] = "i-1234";

int main(int argc, char* argv[]) {
  SDKOptions options;
  InitAPI(options);
  TestCpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  cpio_options.region = kRegion;
  cpio_options.instance_id = kInstanceId;
  auto result = TestLibCpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  ConfigClientOptions config_client_options;
  auto config_client = ConfigClientFactory::Create(move(config_client_options));
  result = config_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init config client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = config_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run config client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }

  atomic<bool> finished = false;
  result = config_client->GetInstanceId(
      GetInstanceIdRequest(),
      [&](const ExecutionResult result, GetInstanceIdResponse response) {
        if (!result.Successful()) {
          std::cout << "GetInstanceId failed: "
                    << GetErrorMessage(result.status_code) << std::endl;
        } else {
          std::cout << "GetInstanceId succeeded, and instance ID is: "
                    << response.instance_id << std::endl;
        }
        finished = true;
      });
  if (!result.Successful()) {
    std::cout << "GetInstanceId failed immediately: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::milliseconds(10000));

  result = config_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop config client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = TestLibCpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
  ShutdownAPI(options);
}
