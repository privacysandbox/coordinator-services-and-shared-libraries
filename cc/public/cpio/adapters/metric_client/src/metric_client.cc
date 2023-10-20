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

#include "metric_client.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include <google/protobuf/util/time_util.h>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/errors.h"
#include "core/utils/src/error_utils.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/metric_client_provider/src/metric_client_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/common/adapter_utils.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::MapPair;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::MetricClientProviderFactory;
using google::scp::cpio::client_providers::MetricClientUtils;
using std::bind;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;

static constexpr char kMetricClient[] = "MetricClient";

namespace google::scp::cpio {
ExecutionResult MetricClient::CreateMetricClientProvider() noexcept {
  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  RETURN_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor(cpu_async_executor));
  shared_ptr<AsyncExecutorInterface> io_async_executor;
  RETURN_IF_FAILURE(
      GlobalCpio::GetGlobalCpio()->GetIoAsyncExecutor(io_async_executor));
  shared_ptr<InstanceClientProviderInterface> instance_client_provider;
  RETURN_IF_FAILURE(GlobalCpio::GetGlobalCpio()->GetInstanceClientProvider(
      instance_client_provider));
  metric_client_provider_ = MetricClientProviderFactory::Create(
      options_, instance_client_provider, cpu_async_executor,
      io_async_executor);

  return SuccessExecutionResult();
}

ExecutionResult MetricClient::Init() noexcept {
  auto execution_result = CreateMetricClientProvider();
  if (!execution_result.Successful()) {
    SCP_ERROR(kMetricClient, kZeroUuid, execution_result,
              "Failed to create MetricClientProvider.");
    return ConvertToPublicExecutionResult(execution_result);
  }

  execution_result = metric_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kMetricClient, kZeroUuid, execution_result,
              "Failed to initialize MetricClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult MetricClient::Run() noexcept {
  auto execution_result = metric_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kMetricClient, kZeroUuid, execution_result,
              "Failed to run MetricClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult MetricClient::Stop() noexcept {
  auto execution_result = metric_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kMetricClient, kZeroUuid, execution_result,
              "Failed to stop MetricClient.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

core::ExecutionResult MetricClient::PutMetrics(
    AsyncContext<PutMetricsRequest, PutMetricsResponse> context) noexcept {
  return metric_client_provider_->PutMetrics(move(context));
}

std::unique_ptr<MetricClientInterface> MetricClientFactory::Create(
    MetricClientOptions options) {
  return make_unique<MetricClient>(make_shared<MetricClientOptions>(options));
}
}  // namespace google::scp::cpio
