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

#include "cpio/client_providers/metric_client_provider/src/utils/aggregate_metric.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "cpio/client_providers/metric_client_provider/mock/mock_metric_client_provider.h"
#include "cpio/client_providers/metric_client_provider/mock/utils/mock_aggregate_metric_with_overrides.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core ::AsyncExecutorInterface;
using google::scp::core::AsyncOperation;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::Timestamp;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_CUSTOMIZED_METRIC_PUSH_CANNOT_SCHEDULE;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockAggregateMetricOverrides;
using google::scp::cpio::client_providers::mock::MockMetricClientProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::mutex;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::to_string;
using std::vector;

namespace google::scp::cpio::client_providers::test {

TEST(AggregateMetricTest, Run) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  vector<ExecutionResult> results = {SuccessExecutionResult(),
                                     FailureExecutionResult(123),
                                     RetryExecutionResult(123)};

  for (auto result : results) {
    auto aggregate_metric = MockAggregateMetricOverrides(
        async_executor, mock_metric_client, metric_info, time_duration);

    aggregate_metric.schedule_metric_push_mock = [&]() { return result; };
    EXPECT_EQ(aggregate_metric.Run(), result);
  }
}

TEST(AggregateMetricTest, ScheduleMetricPush) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  atomic<int> schedule_for_is_called = 0;
  mock_async_executor->schedule_for_mock = [&](const AsyncOperation& work,
                                               Timestamp timestamp,
                                               std::function<bool()>&) {
    schedule_for_is_called++;
    return SuccessExecutionResult();
  };

  auto aggregate_metric = MockAggregateMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration);
  EXPECT_EQ(aggregate_metric.ScheduleMetricPush(),
            FailureExecutionResult(SC_CUSTOMIZED_METRIC_PUSH_CANNOT_SCHEDULE));

  EXPECT_EQ(aggregate_metric.Run(), SuccessExecutionResult());
  EXPECT_EQ(aggregate_metric.ScheduleMetricPush(), SuccessExecutionResult());
  WaitUntil([&]() { return schedule_for_is_called.load() == 2; });
}

TEST(AggregateMetricTest, RunMetricPush) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  vector<string> event_list = {"QPS", "Errors"};

  auto aggregate_metric = MockAggregateMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration,
      make_shared<vector<string>>(event_list));

  int metric_push_handler_is_called = 0;
  int total_counts = 0;
  aggregate_metric.metric_push_handler_mock =
      [&](int64_t counter,
          const std::shared_ptr<MetricTag>& metric_tag = nullptr) {
        metric_push_handler_is_called += 1;
        total_counts += counter;
      };

  for (const auto& code : event_list) {
    aggregate_metric.Increment(code);
    aggregate_metric.Increment();
    EXPECT_EQ(aggregate_metric.GetCounter(code), 1);
  }
  EXPECT_EQ(aggregate_metric.GetCounter(), 2);

  aggregate_metric.RunMetricPush();

  for (const auto& code : event_list) {
    EXPECT_EQ(aggregate_metric.GetCounter(code), 0);
  }
  EXPECT_EQ(aggregate_metric.GetCounter(), 0);
  EXPECT_EQ(metric_push_handler_is_called, 3);
  EXPECT_EQ(total_counts, 4);
}

TEST(AggregateMetricTest, RunMetricPushHandler) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;
  auto counter_value = 1234;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  Metric metric_received;
  int metric_push_is_called = 0;
  mock_metric_client->record_metric_mock =
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        metric_push_is_called += 1;
        metric_received.CopyFrom(context.request->metrics()[0]);
        context.result = FailureExecutionResult(123);
        context.Finish();
        return context.result;
      };

  vector<string> event_list = {"QPS", "Errors"};
  auto aggregate_metric = MockAggregateMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration,
      make_shared<vector<string>>(event_list));

  for (const auto& code : event_list) {
    auto tag = aggregate_metric.GetMetricTag(code);
    aggregate_metric.MetricPushHandler(counter_value, tag);
    EXPECT_EQ(metric_received.name(), *metric_name);
    EXPECT_EQ(metric_received.labels().find("EventCode")->second, code);
    EXPECT_EQ(metric_received.value(), to_string(counter_value));
  }

  aggregate_metric.MetricPushHandler(counter_value);
  EXPECT_EQ(metric_received.name(), *metric_name);
  EXPECT_EQ(metric_received.labels().size(), 0);
  EXPECT_EQ(metric_received.value(), to_string(counter_value));
  WaitUntil([&]() { return metric_push_is_called == 3; });
}

TEST(AggregateMetricTest, Increment) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  vector<string> event_list = {"QPS", "Errors"};
  auto aggregate_metric = MockAggregateMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration,
      make_shared<vector<string>>(event_list));

  auto value = 1;
  for (const auto& code : event_list) {
    for (auto i = 0; i < value; i++) {
      aggregate_metric.Increment(code);
    }
    EXPECT_EQ(aggregate_metric.GetCounter(code), value);
    value++;
  }
}

}  // namespace google::scp::cpio::client_providers::test
