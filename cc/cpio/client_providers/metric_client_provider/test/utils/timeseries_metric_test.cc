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

#include "cpio/client_providers/metric_client_provider/src/utils/timeseries_metric.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/map.h>
#include <google/protobuf/util/message_differencer.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "cpio/client_providers/metric_client_provider/mock/mock_metric_client_provider.h"
#include "cpio/client_providers/metric_client_provider/mock/utils/mock_timeseries_metric_with_overrides.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::core::AsyncContext;
using google::scp::core ::AsyncExecutorInterface;
using google::scp::core::AsyncOperation;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockMetricClientProvider;
using google::scp::cpio::client_providers::mock::MockTimeSeriesMetricOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::mutex;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::unique_ptr;
using std::vector;

namespace google::scp::cpio::client_providers::test {

TEST(TimeSeriesMetricTest, Run) {
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
    mock_async_executor->schedule_for_mock =
        [&](const AsyncOperation& work, Timestamp, std::function<bool()>&) {
          return result;
        };

    auto timeseries_metric = MockTimeSeriesMetricOverrides(
        async_executor, mock_metric_client, metric_info, time_duration);

    EXPECT_EQ(timeseries_metric.Run(), result);
  }
}

TEST(TimeSeriesMetricTest, ScheduleMetricPush) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  bool schedule_for_is_called = false;
  mock_async_executor->schedule_for_mock = [&](const AsyncOperation& work,
                                               Timestamp timestmap,
                                               std::function<bool()>&) {
    schedule_for_is_called = true;
    return SuccessExecutionResult();
  };

  auto timeseries_metric = MockTimeSeriesMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration);

  EXPECT_EQ(timeseries_metric.Run(), SuccessExecutionResult());
  WaitUntil([&]() { return schedule_for_is_called; });
}

TEST(TimeSeriesMetricTest, RunMetricPush) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  bool schedule_for_is_called = false;
  mock_async_executor->schedule_for_mock = [&](const AsyncOperation& work,
                                               Timestamp timestmap,
                                               std::function<bool()>&) {
    work();
    schedule_for_is_called = true;
    return SuccessExecutionResult();
  };

  auto timeseries_metric = MockTimeSeriesMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration);
  bool metric_push_is_called = false;
  timeseries_metric.run_metric_push_mock = [&]() {
    metric_push_is_called = true;
    return;
  };

  EXPECT_EQ(timeseries_metric.Run(), SuccessExecutionResult());
  WaitUntil([&]() { return schedule_for_is_called; });
  WaitUntil([&]() { return metric_push_is_called; });
}

TEST(TimeSeriesMetricTest, MetricPushHandler) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  bool schedule_for_is_called = false;
  mock_async_executor->schedule_for_mock = [&](const AsyncOperation& work,
                                               Timestamp timestmap,
                                               std::function<bool()>&) {
    work();
    schedule_for_is_called = true;
    return SuccessExecutionResult();
  };

  auto timeseries_metric = MockTimeSeriesMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration);
  timeseries_metric.Init();
  bool counter_time_metric_is_called = false;
  bool accumulative_time_metric_is_called = false;
  timeseries_metric.metric_push_handler_mock =
      [&](const shared_ptr<MetricValue>&, const shared_ptr<MetricTag>& tag) {
        if (*tag->update_unit == MetricUnit::kMilliseconds) {
          accumulative_time_metric_is_called = true;
        }
        if (*tag->update_unit == MetricUnit::kCount) {
          counter_time_metric_is_called = true;
        }
        return FailureExecutionResult(SC_UNKNOWN);
      };

  shared_ptr<TimeEvent> time_event = make_shared<TimeEvent>();
  time_event->Stop();
  timeseries_metric.Push(time_event);

  EXPECT_EQ(timeseries_metric.Run(), SuccessExecutionResult());
  WaitUntil([&]() { return schedule_for_is_called; });
  WaitUntil([&]() { return counter_time_metric_is_called; });
  WaitUntil([&]() { return accumulative_time_metric_is_called; });
}

TEST(TimeSeriesMetricTest, MetricPushWithRecordMetric) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  MetricLabels metric_labels;
  metric_labels["TransactionPhase"] = "TestPhase";
  metric_info->labels = make_shared<MetricLabels>(metric_labels);
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  bool schedule_for_is_called = false;
  mock_async_executor->schedule_for_mock = [&](const AsyncOperation& work,
                                               Timestamp timestmap,
                                               std::function<bool()>&) {
    work();
    schedule_for_is_called = true;
    return SuccessExecutionResult();
  };

  auto time_metric_found = false;
  auto counter_metric_found = false;
  mock_metric_client->record_metric_mock =
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        auto type = context.request->metrics()[0].labels().find(string("Type"));
        if (type->second == string("AverageExecutionTime")) {
          time_metric_found = true;
        }
        if (type->second == string("RequestReceived")) {
          counter_metric_found = true;
        }
        return FailureExecutionResult(SC_UNKNOWN);
      };

  auto timeseries_metric = MockTimeSeriesMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration);
  timeseries_metric.Init();
  shared_ptr<TimeEvent> time_event = make_shared<TimeEvent>();
  time_event->Stop();
  timeseries_metric.Push(time_event);

  EXPECT_EQ(timeseries_metric.Run(), SuccessExecutionResult());
  WaitUntil([&]() { return schedule_for_is_called; });
  WaitUntil([&]() { return time_metric_found; });
  WaitUntil([&]() { return counter_metric_found; });
}

TEST(TimeSeriesMetricTest, CounterResetWithMetricPush) {
  auto mock_metric_client = make_shared<MockMetricClientProvider>();
  auto metric_name = make_shared<MetricName>("FrontEndRequestCount");
  auto metric_unit = make_shared<MetricUnit>(MetricUnit::kCount);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  metric_info->name_space = make_shared<MetricNamespace>("PBS");
  auto time_duration = 1000;

  auto mock_async_executor = make_shared<MockAsyncExecutor>();

  shared_ptr<AsyncExecutorInterface> async_executor =
      static_pointer_cast<AsyncExecutorInterface>(mock_async_executor);

  auto timeseries_metric = MockTimeSeriesMetricOverrides(
      async_executor, mock_metric_client, metric_info, time_duration);
  timeseries_metric.Init();
  uint64_t value = 0;
  value += 1;
  shared_ptr<TimeEvent> time_event = make_shared<TimeEvent>();
  sleep(1);
  time_event->Stop();
  timeseries_metric.Push(time_event);
  EXPECT_EQ(timeseries_metric.GetCounter(), value);
  EXPECT_EQ(timeseries_metric.GetAccumulativeTime() / 1000, value);

  value += 1;
  shared_ptr<TimeEvent> time_event_2 = make_shared<TimeEvent>();
  sleep(1);
  time_event_2->Stop();
  timeseries_metric.Push(time_event_2);
  EXPECT_EQ(timeseries_metric.GetCounter(), value);
  EXPECT_EQ(timeseries_metric.GetAccumulativeTime() / 1000, value);

  bool metric_push_handler_is_called = false;
  bool accumulative_time_metric_is_called = false;
  timeseries_metric.metric_push_handler_mock =
      [&](const shared_ptr<MetricValue>& value,
          const shared_ptr<MetricTag>& tag) {
        if (*tag->update_unit == MetricUnit::kMilliseconds) {
          accumulative_time_metric_is_called = true;
        }
        metric_push_handler_is_called = true;
        return FailureExecutionResult(SC_UNKNOWN);
      };

  timeseries_metric.RunMetricPush();

  EXPECT_EQ(timeseries_metric.GetCounter(), 0);
  WaitUntil([&]() { return metric_push_handler_is_called; });
  WaitUntil([&]() { return accumulative_time_metric_is_called; });
}

}  // namespace google::scp::cpio::client_providers::test
