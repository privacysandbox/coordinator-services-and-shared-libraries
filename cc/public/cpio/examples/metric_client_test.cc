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
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::MapPair;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::MetricClientFactory;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MetricClientOptions;
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

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  MetricClientOptions metric_client_options;
  auto metric_client = MetricClientFactory::Create(move(metric_client_options));
  result = metric_client->Init();
  if (!result.Successful()) {
    std::cout << "Cannot init metric client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }
  result = metric_client->Run();
  if (!result.Successful()) {
    std::cout << "Cannot run metric client!"
              << GetErrorMessage(result.status_code) << std::endl;
    return 0;
  }

  auto request = make_shared<PutMetricsRequest>();
  request->set_metric_namespace("test");
  auto metric = request->add_metrics();
  metric->set_name("test_metric");
  metric->set_value("12");
  metric->set_unit(MetricUnit::METRIC_UNIT_COUNT);
  auto labels = metric->mutable_labels();
  labels->insert(MapPair(string("lable_key"), string("label_value")));

  atomic<bool> finished = false;
  auto context = AsyncContext<PutMetricsRequest, PutMetricsResponse>(
      move(request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse> context) {
        if (!context.result.Successful()) {
          std::cout << "PutMetrics failed: "
                    << GetErrorMessage(context.result.status_code) << std::endl;
        } else {
          std::cout << "PutMetrics succeeded." << std::endl;
        }
        finished = true;
      });
  result = metric_client->PutMetrics(context);
  if (!result.Successful()) {
    std::cout << "PutMetrics failed immediately: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
  WaitUntil([&finished]() { return finished.load(); },
            std::chrono::milliseconds(100000));

  result = metric_client->Stop();
  if (!result.Successful()) {
    std::cout << "Cannot stop metric client!"
              << GetErrorMessage(result.status_code) << std::endl;
  }

  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
