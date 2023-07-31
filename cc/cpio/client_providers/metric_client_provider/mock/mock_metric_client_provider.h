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
#pragma once

#include <memory>
#include <vector>

#include <google/protobuf/util/message_differencer.h>

#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockMetricClientProvider : public MetricClientProviderInterface {
 public:
  core::ExecutionResult init_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Init() noexcept override { return init_result_mock; }

  core::ExecutionResult run_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Run() noexcept override { return run_result_mock; }

  core::ExecutionResult stop_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Stop() noexcept override { return stop_result_mock; }

  std::function<core::ExecutionResult(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>&)>
      record_metric_mock;

  core::ExecutionResult record_metric_result_mock;
  cmrt::sdk::metric_service::v1::PutMetricsRequest record_metrics_request_mock;

  core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult PutMetrics(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>&
          context) noexcept override {
    if (record_metric_mock) {
      return record_metric_mock(context);
    }
    google::protobuf::util::MessageDifferencer differencer;
    differencer.set_repeated_field_comparison(
        google::protobuf::util::MessageDifferencer::AS_SET);
    if (differencer.Equals(
            record_metrics_request_mock,
            cmrt::sdk::metric_service::v1::PutMetricsRequest()) ||
        differencer.Equals(record_metrics_request_mock,
                           ZeroTimestampe(context.request))) {
      context.result = record_metric_result_mock;
      if (record_metric_result_mock == core::SuccessExecutionResult()) {
        context.response = std::make_shared<
            cmrt::sdk::metric_service::v1::PutMetricsResponse>();
      }
      context.Finish();
    }
    return record_metric_result_mock;
  }

  // TODO(b/253115895): figure out why IgnoreField doesn't work for
  // MessageDifferencer.
  cmrt::sdk::metric_service::v1::PutMetricsRequest ZeroTimestampe(
      std::shared_ptr<cmrt::sdk::metric_service::v1::PutMetricsRequest>&
          request) {
    cmrt::sdk::metric_service::v1::PutMetricsRequest output;
    output.CopyFrom(*request);
    for (auto metric = output.mutable_metrics()->begin();
         metric < output.mutable_metrics()->end(); metric++) {
      metric->set_timestamp_in_ms(0);
    }
    return output;
  }
};
}  // namespace google::scp::cpio::client_providers::mock
