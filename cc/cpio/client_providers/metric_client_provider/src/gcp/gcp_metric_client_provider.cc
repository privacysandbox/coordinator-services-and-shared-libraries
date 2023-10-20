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

#include "gcp_metric_client_provider.h"

#include <string>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "google/cloud/future.h"
#include "google/cloud/monitoring/metric_client.h"
#include "google/cloud/monitoring/metric_connection.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"
#include "gcp_metric_client_utils.h"

using google::cloud::future;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::monitoring::MakeMetricServiceConnection;
using google::cloud::monitoring::MetricServiceClient;
using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::monitoring::v3::CreateTimeSeriesRequest;
using google::monitoring::v3::TimeSeries;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::client_providers::GcpInstanceResourceNameDetails;
using google::scp::cpio::client_providers::GcpMetricClientUtils;
using google::scp::cpio::common::GcpUtils;
using std::bind;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;

static constexpr char kGcpMetricClientProvider[] = "GcpMetricClientProvider";

// The limit of GCP metric client time series list size is 200.
static constexpr size_t kGcpTimeSeriesSizeLimit = 200;

namespace google::scp::cpio::client_providers {

ExecutionResult GcpMetricClientProvider::Run() noexcept {
  auto execution_result = MetricClientProvider::Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpMetricClientProvider, kZeroUuid, execution_result,
              "Failed to initialize MetricClientProvider");
    return execution_result;
  }

  string instance_resource_name;
  execution_result =
      instance_client_provider_->GetCurrentInstanceResourceNameSync(
          instance_resource_name);
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpMetricClientProvider, kZeroUuid, execution_result,
              "Failed to fetch current instance resource name");
    return execution_result;
  }

  execution_result = GcpInstanceClientUtils::GetInstanceResourceNameDetails(
      instance_resource_name, instance_resource_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpMetricClientProvider, kZeroUuid, execution_result,
              "Failed to parse instance resource name %s",
              instance_resource_name.c_str());
  }

  CreateMetricServiceClient();

  return SuccessExecutionResult();
}

void GcpMetricClientProvider::CreateMetricServiceClient() noexcept {
  auto metric_service_connection = MakeMetricServiceConnection();

  metric_service_client_ =
      make_shared<MetricServiceClient>(metric_service_connection);
}

ExecutionResult GcpMetricClientProvider::MetricsBatchPush(
    const shared_ptr<
        vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>&
        context_vector) noexcept {
  MetricServiceClient metric_client(*metric_service_client_);
  CreateTimeSeriesRequest time_series_request;

  // Sets the name for time series request.
  time_series_request.set_name(GcpMetricClientUtils::ConstructProjectName(
      instance_resource_.project_id));

  // Chops the input context_vector to small piece of vector, and
  // requests_vector is used in callback function to set the response for
  // requests.
  auto requests_vector =
      make_shared<vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>(
          kGcpTimeSeriesSizeLimit);

  // When batch recording is not enabled, expect the namespace to be set on the
  // request. context_vector won't be empty.
  auto name_space = metric_batching_options_->enable_batch_recording
                        ? metric_batching_options_->metric_namespace
                        : context_vector->back().request->metric_namespace();

  while (!context_vector->empty()) {
    auto context = context_vector->back();
    context_vector->pop_back();
    vector<TimeSeries> time_series_list;
    auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
        context, name_space, time_series_list);
    // Sets the result for the requests that failed in parsing to time series.
    if (!result.Successful()) {
      context.result = result;
      context.Finish();
      continue;
    }

    // Add gce_instance resource info to TimeSeries data.
    GcpMetricClientUtils::AddResourceToTimeSeries(
        instance_resource_.project_id, instance_resource_.instance_id,
        instance_resource_.zone_id, time_series_list);

    requests_vector->push_back(context);
    time_series_request.mutable_time_series()->Add(time_series_list.begin(),
                                                   time_series_list.end());

    // Calls Gcp CreateTimeSeries when time series size equals to 200 or context
    // vector is empty.
    if (time_series_request.time_series().size() == kGcpTimeSeriesSizeLimit ||
        context_vector->empty()) {
      metric_client.AsyncCreateTimeSeries(time_series_request)
          .then(std::bind(
              &GcpMetricClientProvider::OnAsyncCreateTimeSeriesCallback, this,
              *requests_vector, _1));
      active_push_count_++;

      // Clear requests_vector and protobuf repeated field.
      time_series_request.mutable_time_series()->Clear();
      requests_vector->clear();
    }
  }

  return SuccessExecutionResult();
}

// Copy the metric_requests_vector in case it is cleared outside, and it is not
// expensive to copy the AsyncContext.
void GcpMetricClientProvider::OnAsyncCreateTimeSeriesCallback(
    vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>
        metric_requests_vector,
    future<Status> outcome) noexcept {
  active_push_count_--;
  auto outcome_status = outcome.get();
  auto result = GcpUtils::GcpErrorConverter(outcome_status);

  if (!result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpMetricClientProvider, metric_requests_vector.back(),
                      result, "The error is %s",
                      outcome_status.message().c_str());
  }

  for (auto& record_metric_context : metric_requests_vector) {
    record_metric_context.response = make_shared<PutMetricsResponse>();
    record_metric_context.result = result;
    record_metric_context.Finish();
  }
  return;
}

std::shared_ptr<MetricClientInterface> MetricClientProviderFactory::Create(
    const shared_ptr<MetricClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return make_shared<GcpMetricClientProvider>(options, instance_client_provider,
                                              async_executor);
}
}  // namespace google::scp::cpio::client_providers
