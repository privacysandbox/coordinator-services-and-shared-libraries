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
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/instance_client/instance_client_interface.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::InstanceClientFactory;
using google::scp::cpio::InstanceClientInterface;
using google::scp::cpio::InstanceClientOptions;
using google::scp::cpio::LogOption;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::placeholders::_1;
using std::placeholders::_2;

unique_ptr<InstanceClientInterface> instance_client;

void GetTagsByResourceNameCallback(
    atomic<bool>& finished, ExecutionResult result,
    GetTagsByResourceNameResponse get_tags_response) {
  if (!result.Successful()) {
    std::cout << "GetTagsByResourceName failed: "
              << GetErrorMessage(result.status_code) << std::endl;
  } else {
    std::cout << "GetTagsByResourceName succeeded, and the tags are: "
              << std::endl;
    for (const auto& tag : get_tags_response.tags()) {
      std::cout << tag.first << " : " << tag.second << std::endl;
    }
  }
  finished = true;
}

void GetCurrentInstanceResourceNameCallback(
    atomic<bool>& finished, ExecutionResult result,
    GetCurrentInstanceResourceNameResponse get_resource_name_response) {
  if (!result.Successful()) {
    std::cout << "Hpke encrypt failure!" << GetErrorMessage(result.status_code)
              << std::endl;
    return;
  }

  std::cout << "GetCurrentInstanceResourceName succeeded, and the "
               "instance resource name is: "
            << get_resource_name_response.instance_resource_name() << std::endl;

  GetTagsByResourceNameRequest get_tags_request;
  get_tags_request.set_resource_name(
      get_resource_name_response.instance_resource_name());
  result = instance_client->GetTagsByResourceName(
      move(get_tags_request),
      bind(GetTagsByResourceNameCallback, std::ref(finished), _1, _2));
  if (!result.Successful()) {
    std::cout << "GetTagsByResourceName failed immediately!"
              << GetErrorMessage(result.status_code) << std::endl;
  }
}

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  InstanceClientOptions instance_client_options;
  instance_client =
      InstanceClientFactory::Create(move(instance_client_options));
  result = instance_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init instance client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = instance_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run instance client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }

  atomic<bool> finished = false;
  result = instance_client->GetCurrentInstanceResourceName(
      GetCurrentInstanceResourceNameRequest(),
      bind(GetCurrentInstanceResourceNameCallback, std::ref(finished), _1, _2));

  if (!result.Successful()) {
    std::cout << "GetCurrentInstanceResourceName failed immediately: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::milliseconds(3000));

  result = instance_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop instance client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
