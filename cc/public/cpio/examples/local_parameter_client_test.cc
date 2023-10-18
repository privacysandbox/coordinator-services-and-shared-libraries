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

#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "public/cpio/test/global_cpio/test_lib_cpio.h"

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::LogOption;
using google::scp::cpio::ParameterClientFactory;
using google::scp::cpio::ParameterClientInterface;
using google::scp::cpio::ParameterClientOptions;
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
static constexpr char kTestParameterName[] = "test_parameter";

int main(int argc, char* argv[]) {
  TestCpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  cpio_options.region = kRegion;
  auto result = TestLibCpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  ParameterClientOptions parameter_client_options;
  auto parameter_client =
      ParameterClientFactory::Create(move(parameter_client_options));
  result = parameter_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init parameter client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = parameter_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run parameter client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }

  atomic<bool> finished = false;
  GetParameterRequest get_parameter_request;
  get_parameter_request.set_parameter_name(kTestParameterName);
  result = parameter_client->GetParameter(
      move(get_parameter_request),
      [&](const ExecutionResult result, GetParameterResponse response) {
        if (!result.Successful()) {
          std::cout << "GetParameter failed: "
                    << GetErrorMessage(result.status_code) << std::endl;
        } else {
          std::cout << "GetParameter succeeded, and parameter is: "
                    << response.parameter_value() << std::endl;
        }
        finished = true;
      });
  if (!result.Successful()) {
    std::cout << "GetParameter failed immediately: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::milliseconds(10000));

  result = parameter_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop parameter client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = TestLibCpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
