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

#include "public/cpio/adapters/metric_client/src/metric_client.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <gmock/gmock.h>

#include "core/interface/errors.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/metric_client_provider/mock/mock_metric_client_provider_with_overrides.h"
#include "cpio/client_providers/metric_client_provider/src/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/metric_client/mock/mock_metric_client_with_overrides.h"
#include "public/cpio/core/mock/mock_lib_cpio.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::protobuf::MapPair;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CPIO_INVALID_REQUEST;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::mock::MockMetricClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

static constexpr char kName[] = "test_name";
static constexpr char kValue[] = "1234.90";
static const map<string, string> kLabels = {{"a", "10"}, {"b", "20"}};
static constexpr char kNamespace[] = "name_space";

namespace google::scp::cpio::test {
class MetricClientTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
    InitCpio();
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
    ShutdownCpio();
  }

  void SetUp() override {
    auto metric_client_options = make_shared<MetricClientOptions>();
    metric_client_options->metric_namespace = kNamespace;
    client_ = make_unique<MockMetricClientWithOverrides>(metric_client_options);
  }

  void AddMetric(PutMetricsRequest& request) {
    Metric request_metric;
    request_metric.name = kName;
    request_metric.value = kValue;
    request_metric.unit = MetricUnit::kCount;
    request_metric.labels = kLabels;
    request.metrics.push_back(request_metric);
  }

  void AddMetricProto(
      cmrt::sdk::metric_service::v1::PutMetricsRequest& request) {
    auto metric = request.add_metrics();
    metric->set_name(kName);
    metric->set_value(kValue);
    auto labels = metric->mutable_labels();
    for (const auto& label : kLabels) {
      labels->insert(MapPair<string, string>(label.first, label.second));
    }
    metric->set_unit(
        cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_COUNT);
  }

  unique_ptr<MockMetricClientWithOverrides> client_;
};

TEST_F(MetricClientTest, RecordMetricRequestSuccess) {
  auto expected_result = SuccessExecutionResult();
  client_->GetMetricClientProvider()->record_metric_result_mock =
      expected_result;
  cmrt::sdk::metric_service::v1::PutMetricsRequest proto_request;
  AddMetricProto(proto_request);
  client_->GetMetricClientProvider()->record_metrics_request_mock =
      proto_request;
  PutMetricsRequest request;
  AddMetric(request);
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());
  atomic<bool> condition = false;
  EXPECT_EQ(client_->PutMetrics(
                request,
                [&](const ExecutionResult result, PutMetricsResponse response) {
                  EXPECT_EQ(result, SuccessExecutionResult());
                  condition = true;
                }),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(MetricClientTest, MultipleMetrics) {
  auto expected_result = SuccessExecutionResult();
  client_->GetMetricClientProvider()->record_metric_result_mock =
      expected_result;
  cmrt::sdk::metric_service::v1::PutMetricsRequest proto_request;
  AddMetricProto(proto_request);
  AddMetricProto(proto_request);
  client_->GetMetricClientProvider()->record_metrics_request_mock =
      proto_request;
  PutMetricsRequest request;
  AddMetric(request);
  AddMetric(request);
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());
  atomic<bool> condition = false;
  EXPECT_EQ(client_->PutMetrics(
                request,
                [&](const ExecutionResult result, PutMetricsResponse response) {
                  EXPECT_EQ(result, SuccessExecutionResult());
                  condition = true;
                }),
            SuccessExecutionResult());
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(MetricClientTest, RecordMetricRequestFailure) {
  auto expected_result =
      FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET);
  client_->GetMetricClientProvider()->record_metric_result_mock =
      expected_result;
  auto public_error = FailureExecutionResult(SC_CPIO_INVALID_REQUEST);
  PutMetricsRequest request;
  AddMetric(request);
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());
  atomic<bool> condition = false;
  EXPECT_EQ(client_->PutMetrics(
                request,
                [&](const ExecutionResult result, PutMetricsResponse response) {
                  EXPECT_EQ(result, public_error);
                  condition = true;
                }),
            public_error);
  WaitUntil([&]() { return condition.load(); });
  EXPECT_EQ(client_->Stop(), SuccessExecutionResult());
}

TEST_F(MetricClientTest, InitFailure) {
  auto expected_result =
      FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET);
  client_->GetMetricClientProvider()->init_result_mock = expected_result;
  auto public_error = FailureExecutionResult(SC_CPIO_INVALID_REQUEST);
  EXPECT_EQ(client_->Init(), public_error);
}

TEST_F(MetricClientTest, RunFailure) {
  auto expected_result =
      FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET);
  client_->GetMetricClientProvider()->run_result_mock = expected_result;
  auto public_error = FailureExecutionResult(SC_CPIO_INVALID_REQUEST);
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), public_error);
}

TEST_F(MetricClientTest, StopFailure) {
  auto expected_result =
      FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET);
  client_->GetMetricClientProvider()->stop_result_mock = expected_result;
  auto public_error = FailureExecutionResult(SC_CPIO_INVALID_REQUEST);
  EXPECT_EQ(client_->Init(), SuccessExecutionResult());
  EXPECT_EQ(client_->Run(), SuccessExecutionResult());
  EXPECT_EQ(client_->Stop(), public_error);
}
}  // namespace google::scp::cpio::test
