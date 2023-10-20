
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

#include "cpio/client_providers/metric_client_provider/src/metric_client_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <aws/core/Aws.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/metric_client_provider/mock/mock_metric_client_provider_with_overrides.h"
#include "cpio/client_providers/metric_client_provider/src/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core ::AsyncExecutorInterface;
using google::scp::core::AsyncOperation;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_METRIC_CLIENT_PROVIDER_EXECUTOR_NOT_AVAILABLE;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_IS_NOT_RUNNING;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockMetricClientWithOverrides;
using std::atomic;
using std::function;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;

namespace {
constexpr size_t kMetricsBatchSize = 1000;
constexpr char kMetricNamespace[] = "test";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class MetricClientProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  shared_ptr<MetricBatchingOptions> CreateMetricBatchingOptions(
      bool enable_batch_recording, string metric_namespace = kMetricNamespace) {
    auto options = make_shared<MetricBatchingOptions>();
    options->metric_namespace = metric_namespace;
    options->enable_batch_recording = enable_batch_recording;
    return options;
  }

  shared_ptr<PutMetricsRequest> CreatePutMetricsRequest(
      const string& metric_namespace = "") {
    auto request = make_shared<PutMetricsRequest>();
    request->set_metric_namespace(metric_namespace);
    auto metric = request->add_metrics();
    metric->set_name("metric1");
    metric->set_value("123");
    return request;
  }

  shared_ptr<MockAsyncExecutor> mock_async_executor_ =
      make_shared<MockAsyncExecutor>();
};

TEST_F(MetricClientProviderTest, EmptyAsyncExecutorIsNotOKWithBatchRecording) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      nullptr, CreateMetricBatchingOptions(true));
  EXPECT_THAT(client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_EXECUTOR_NOT_AVAILABLE)));
}

TEST_F(MetricClientProviderTest, EmptyAsyncExecutorIsOKWithoutBatchRecording) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      nullptr, CreateMetricBatchingOptions(false));

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      CreatePutMetricsRequest(kMetricNamespace),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  int64_t batch_push_called_count = 0;
  client->metrics_batch_push_mock =
      [&](const std::shared_ptr<std::vector<core::AsyncContext<
              cmrt::sdk::metric_service::v1::PutMetricsRequest,
              cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        EXPECT_EQ(metric_requests_vector->size(), 1);
        batch_push_called_count += 1;
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());
  EXPECT_SUCCESS(client->PutMetrics(context));
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  EXPECT_SUCCESS(client->PutMetrics(context));
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  WaitUntil([&]() { return batch_push_called_count == 2; });
}

TEST_F(MetricClientProviderTest,
       FailsWhenEnableBatchRecordingWithoutNamespace) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      mock_async_executor_, CreateMetricBatchingOptions(true, ""));
  auto result = client->Init();
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET)));
}

TEST_F(MetricClientProviderTest,
       FailsWithoutNamespaceInRequestWhenNoBatchRecording) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      mock_async_executor_, CreateMetricBatchingOptions(false));

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());
  EXPECT_THAT(client->PutMetrics(context),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET)));
  EXPECT_SUCCESS(client->Stop());
}

TEST_F(MetricClientProviderTest, FailsWhenNoMetricInRequest) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      mock_async_executor_, CreateMetricBatchingOptions(false));

  auto request = make_shared<PutMetricsRequest>();
  request->set_metric_namespace(kMetricNamespace);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      move(request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());
  EXPECT_THAT(client->PutMetrics(context),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET)));
  EXPECT_SUCCESS(client->Stop());
}

TEST_F(MetricClientProviderTest, FailedWithoutRunning) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      mock_async_executor_, CreateMetricBatchingOptions(true));

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      make_shared<PutMetricsRequest>(),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  EXPECT_SUCCESS(client->Init());
  EXPECT_THAT(client->ScheduleMetricsBatchPush(),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_IS_NOT_RUNNING)));
  EXPECT_THAT(client->PutMetrics(context),
              ResultIs(FailureExecutionResult(
                  SC_METRIC_CLIENT_PROVIDER_IS_NOT_RUNNING)));
}

TEST_F(MetricClientProviderTest, LaunchScheduleMetricsBatchPushWithRun) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      mock_async_executor_, CreateMetricBatchingOptions(true));

  bool schedule_for_is_called = false;
  mock_async_executor_->schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          function<bool()>& cancellation_callback) {
        schedule_for_is_called = true;
        return FailureExecutionResult(SC_UNKNOWN);
      };

  EXPECT_SUCCESS(client->Init());
  EXPECT_THAT(client->Run(), ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return schedule_for_is_called; });
}

TEST_F(MetricClientProviderTest, RecordMetricWithoutBatch) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      mock_async_executor_, CreateMetricBatchingOptions(false));

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      CreatePutMetricsRequest(kMetricNamespace),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  int64_t batch_push_called_count = 0;
  client->metrics_batch_push_mock =
      [&](const std::shared_ptr<std::vector<core::AsyncContext<
              cmrt::sdk::metric_service::v1::PutMetricsRequest,
              cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        EXPECT_EQ(metric_requests_vector->size(), 1);
        batch_push_called_count += 1;
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());
  EXPECT_SUCCESS(client->PutMetrics(context));
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  EXPECT_SUCCESS(client->PutMetrics(context));
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  WaitUntil([&]() { return batch_push_called_count == 2; });
}

TEST_F(MetricClientProviderTest, RecordMetricWithBatch) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      mock_async_executor_, CreateMetricBatchingOptions(true));

  atomic<bool> schedule_for_is_called = false;
  mock_async_executor_->schedule_for_mock =
      [&](const AsyncOperation& work, Timestamp timestamp,
          function<bool()>& cancellation_callback) {
        schedule_for_is_called = true;
        return SuccessExecutionResult();
      };

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      CreatePutMetricsRequest(),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  atomic<bool> batch_push_called = false;
  client->metrics_batch_push_mock =
      [&](const std::shared_ptr<std::vector<core::AsyncContext<
              cmrt::sdk::metric_service::v1::PutMetricsRequest,
              cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        batch_push_called.store(true);
        EXPECT_EQ(metric_requests_vector->size(), kMetricsBatchSize);
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

  for (int i = 0; i <= 2001; i++) {
    EXPECT_SUCCESS(client->PutMetrics(context));
  }

  WaitUntil([&]() { return schedule_for_is_called.load(); });

  WaitUntil([&]() { return batch_push_called.load(); });
}

TEST_F(MetricClientProviderTest, RunMetricsBatchPush) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      mock_async_executor_, CreateMetricBatchingOptions(true));

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      CreatePutMetricsRequest(),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  int64_t schedule_metric_push_count = 0;
  client->schedule_metric_push_mock = [&]() {
    schedule_metric_push_count += 1;
    return SuccessExecutionResult();
  };

  int64_t batch_push_called_count = 0;
  client->metrics_batch_push_mock =
      [&](const std::shared_ptr<std::vector<core::AsyncContext<
              cmrt::sdk::metric_service::v1::PutMetricsRequest,
              cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        EXPECT_EQ(metric_requests_vector->size(), 2);
        batch_push_called_count += 1;
        return SuccessExecutionResult();
      };

  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

  EXPECT_SUCCESS(client->PutMetrics(context));
  EXPECT_SUCCESS(client->PutMetrics(context));
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 2);
  client->RunMetricsBatchPush();
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  WaitUntil([&]() { return batch_push_called_count == 1; });
  WaitUntil([&]() { return schedule_metric_push_count == 1; });
}

TEST_F(MetricClientProviderTest, PutMetricSuccessWithMultipleThreads) {
  auto client = make_unique<MockMetricClientWithOverrides>(
      nullptr, CreateMetricBatchingOptions(false));
  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());
  auto request = CreatePutMetricsRequest(kMetricNamespace);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      move(request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  atomic<int> batch_push_called_count = 0;
  client->metrics_batch_push_mock =
      [&](const shared_ptr<
          vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        EXPECT_EQ(metric_requests_vector->size(), 1);
        batch_push_called_count++;
        return SuccessExecutionResult();
      };
  vector<thread> threads;
  for (auto i = 0; i < 100; ++i) {
    threads.push_back(
        thread([&]() { EXPECT_SUCCESS(client->PutMetrics(context)); }));
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  WaitUntil([&]() { return batch_push_called_count.load() == 100; });
  EXPECT_SUCCESS(client->Stop());
}

}  // namespace google::scp::cpio::client_providers::test
