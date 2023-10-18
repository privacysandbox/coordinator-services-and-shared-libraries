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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/adapters/metric_client/mock/mock_metric_client_with_overrides.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::MetricClient;
using google::scp::cpio::MetricClientOptions;
using google::scp::cpio::mock::MockMetricClientWithOverrides;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::Return;

namespace google::scp::cpio::test {
class MetricClientTest : public ::testing::Test {
 protected:
  MetricClientTest() {
    auto metric_client_options = make_shared<MetricClientOptions>();
    client_ = make_unique<MockMetricClientWithOverrides>(metric_client_options);

    EXPECT_THAT(client_->Init(), IsSuccessful());
    EXPECT_THAT(client_->Run(), IsSuccessful());
  }

  ~MetricClientTest() { EXPECT_THAT(client_->Stop(), IsSuccessful()); }

  unique_ptr<MockMetricClientWithOverrides> client_;
};

TEST_F(MetricClientTest, PutMetricsSuccess) {
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context;
  EXPECT_CALL(*client_->GetMetricClientProvider(), PutMetrics)
      .WillOnce(Return(SuccessExecutionResult()));
  EXPECT_THAT(client_->PutMetrics(context), IsSuccessful());
}

TEST_F(MetricClientTest, FailureToCreateMetricClientProvider) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_->create_metric_client_provider_result = failure;
  EXPECT_EQ(client_->Init(), failure);
}
}  // namespace google::scp::cpio::test
