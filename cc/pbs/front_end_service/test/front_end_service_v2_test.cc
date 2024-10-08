// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cc/pbs/front_end_service/src/front_end_service_v2.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "cc/core/async_executor/mock/mock_async_executor.h"
#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/http2_server/mock/mock_http2_server.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/core/telemetry/mock/in_memory_metric_router.h"
#include "cc/core/telemetry/mock/instrument_mock.h"
#include "cc/core/telemetry/src/common/metric_utils.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/front_end_service/src/metric_initialization.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/pbs/partition_request_router/mock/mock_transaction_request_router.h"
#include "cc/pbs/transactions/mock/mock_consume_budget_command_factory.h"
#include "cc/public/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/metric_client/metric_client_interface.h"
#include "cc/public/cpio/mock/metric_client/mock_metric_client.h"
#include "cc/public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "cc/public/cpio/utils/metric_aggregation/mock/mock_aggregate_metric.h"

namespace google::scp::pbs {

using ::google::scp::core::AsyncContext;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::HttpRequest;
using ::google::scp::core::HttpResponse;
using ::google::scp::cpio::AggregateMetricInterface;
using ::google::scp::cpio::MockAggregateMetric;

class FrontEndServiceV2Peer {
 public:
  explicit FrontEndServiceV2Peer(
      std::unique_ptr<FrontEndServiceV2> front_end_service_v2)
      : front_end_service_v2_(std::move(front_end_service_v2)) {}

  ExecutionResult BeginTransaction(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    return front_end_service_v2_->BeginTransaction(http_context);
  }

  ExecutionResult PrepareTransaction(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    return front_end_service_v2_->PrepareTransaction(http_context);
  }

  ExecutionResult CommitTransaction(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    return front_end_service_v2_->CommitTransaction(http_context);
  }

  ExecutionResult NotifyTransaction(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    return front_end_service_v2_->NotifyTransaction(http_context);
  }

  ExecutionResult AbortTransaction(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    return front_end_service_v2_->AbortTransaction(http_context);
  }

  ExecutionResult EndTransaction(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    return front_end_service_v2_->EndTransaction(http_context);
  }

  ExecutionResult GetTransactionStatus(
      AsyncContext<HttpRequest, HttpResponse>& http_context) {
    return front_end_service_v2_->GetTransactionStatus(http_context);
  }

  ExecutionResult Init() { return front_end_service_v2_->Init(); }

  // Downcasts AggregateMetricInterface to MockAggregateMetric, which we inject
  // into a FrontEndServiceV2 for testing.
  std::shared_ptr<MockAggregateMetric> GetMetricsInstance(
      absl::string_view method_name, absl::string_view phase) {
    std::shared_ptr<AggregateMetricInterface> metrics_instance =
        front_end_service_v2_->metrics_instances_map_.at(method_name).at(phase);
    return std::dynamic_pointer_cast<MockAggregateMetric>(metrics_instance);
  }

 private:
  std::unique_ptr<FrontEndServiceV2> front_end_service_v2_;
};

namespace {

using ::google::scp::core::AsyncContext;
using ::google::scp::core::AsyncExecutorInterface;
using ::google::scp::core::Byte;
using ::google::scp::core::ConfigProviderInterface;
using ::google::scp::core::ExecutionResultOr;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::HttpHandler;
using ::google::scp::core::HttpHeaders;
using ::google::scp::core::HttpMethod;
using ::google::scp::core::HttpServerInterface;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::async_executor::mock::MockAsyncExecutor;
using ::google::scp::core::config_provider::mock::MockConfigProvider;
using ::google::scp::core::errors::
    SC_PBS_FRONT_END_SERVICE_GET_TRANSACTION_STATUS_RETURNS_404_BY_DEFAULT;
using ::google::scp::core::errors::
    SC_PBS_FRONT_END_SERVICE_UNABLE_TO_FIND_TRANSACTION_METRICS;
using ::google::scp::cpio::MetricClientInterface;
using ::google::scp::cpio::MockMetricClient;
using ::google::scp::pbs::errors::SC_CONSUME_BUDGET_EXHAUSTED;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

static constexpr absl::string_view kTransactionId =
    "3E2A3D09-48ED-A355-D346-AD7DC6CB0909";
static constexpr absl::string_view kTransactionSecret = "secret";
static constexpr absl::string_view kReportingOrigin = "https://fake.com";
static constexpr absl::string_view kRequestBody = R"({
        "v": "1.0",
        "t": [
            {
                "key": "test_key",
                "token": 10,
                "reporting_time": "2019-10-12T07:20:50.52Z"
            },
            {
                "key": "test_key_2",
                "token": 23,
                "reporting_time": "2019-12-12T07:20:50.52Z"
            }
        ]
    })";
static constexpr absl::string_view kBudgetExhaustedResponseBody =
    R"({"f":[0],"v":"1.0"})";

class MockBudgetConsumptionHelper : public BudgetConsumptionHelperInterface {
 public:
  MOCK_METHOD(ExecutionResult, ConsumeBudgets,
              ((google::scp::core::AsyncContext<ConsumeBudgetsRequest,
                                                ConsumeBudgetsResponse>
                    consume_budgets_context)),
              (noexcept, override));
  MOCK_METHOD(ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(ExecutionResult, Run, (), (noexcept, override));
  MOCK_METHOD(ExecutionResult, Stop, (), (noexcept, override));
};

class MockHttpServerInterface : public NiceMock<HttpServerInterface> {
 public:
  MOCK_METHOD(ExecutionResult, Init, (), (noexcept, override));
  MOCK_METHOD(ExecutionResult, Run, (), (noexcept, override));
  MOCK_METHOD(ExecutionResult, Stop, (), (noexcept, override));
  MOCK_METHOD(ExecutionResult, RegisterResourceHandler,
              (HttpMethod http_method, std::string& resource_path,
               HttpHandler& handler),
              (noexcept, override));
};

class MockMetricInitialization : public NiceMock<MetricInitialization> {
 public:
  MockMetricInitialization() {
    ON_CALL(*this, Initialize(_, _, _))
        .WillByDefault(Invoke(&MockMetricInitialization::InitializeWithMock));
  }

  MOCK_METHOD(ExecutionResultOr<MetricsMap>, Initialize,
              (std::shared_ptr<AsyncExecutorInterface> async_executor,
               std::shared_ptr<MetricClientInterface> metric_client,
               core::TimeDuration aggregated_metric_interval_ms),
              (const, noexcept, override));

 private:
  static ExecutionResultOr<MetricsMap> InitializeWithMock() {
    MetricsMap metrics_map;
    for (absl::string_view method_name : kMetricInitializationMethodNames) {
      for (absl::string_view metric_name : kMetricInitializationMetricNames) {
        metrics_map[method_name][metric_name] =
            std::make_shared<MockAggregateMetric>();
      }
    }
    return metrics_map;
  }
};

struct FrontEndServiceV2PeerOptions {
  std::optional<std::shared_ptr<MockHttpServerInterface>> http_server =
      std::nullopt;
  std::optional<std::shared_ptr<AsyncExecutorInterface>> async_executor =
      std::nullopt;
  std::optional<std::shared_ptr<MetricClientInterface>> metric_client =
      std::nullopt;
  std::optional<std::shared_ptr<ConfigProviderInterface>> config_provider =
      std::nullopt;
  std::optional<std::unique_ptr<MetricInitialization>> metric_initialization =
      std::nullopt;
  BudgetConsumptionHelperInterface* budget_consumption_helper = nullptr;

  // for testing otel metrics
  std::shared_ptr<core::config_provider::mock::MockConfigProvider>
      mock_config_provider;
  std::unique_ptr<core::InMemoryMetricRouter> metric_router;
};

FrontEndServiceV2Peer MakeFrontEndServiceV2Peer(
    FrontEndServiceV2PeerOptions& options) {
  if (!options.config_provider.has_value()) {
    std::shared_ptr<MockConfigProvider> mock_config_provider =
        std::make_shared<MockConfigProvider>();
    static constexpr char kClaimedIdentity[] = "123";
    mock_config_provider->Set(kRemotePrivacyBudgetServiceClaimedIdentity,
                              kClaimedIdentity);
    options.config_provider = mock_config_provider;
  }
  options.metric_router = std::make_unique<core::InMemoryMetricRouter>();

  std::unique_ptr<FrontEndServiceV2> front_end_service_v2 =
      std::make_unique<FrontEndServiceV2>(
          options.http_server.value_or(
              std::make_shared<MockHttpServerInterface>()),
          options.async_executor.value_or(
              std::make_shared<MockAsyncExecutor>()),
          options.metric_client.value_or(std::make_shared<MockMetricClient>()),
          options.config_provider.value(), options.budget_consumption_helper,
          options.metric_initialization.has_value()
              ? std::move(options.metric_initialization.value())
              : std::make_unique<MockMetricInitialization>());
  return FrontEndServiceV2Peer(std::move(front_end_service_v2));
}

void InsertCommonHeaders(
    absl::string_view transaction_id, absl::string_view secret,
    absl::string_view autorized_domain,
    AsyncContext<HttpRequest, HttpResponse>& http_context) {
  http_context.request->headers = std::make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {std::string(kTransactionIdHeader), std::string(transaction_id)});
  http_context.request->headers->insert(
      {std::string(kTransactionSecretHeader), std::string(secret)});
  http_context.request->headers->insert(
      {std::string(kTransactionLastExecutionTimestampHeader), "123"});
  http_context.request->auth_context.authorized_domain =
      std::make_shared<std::string>(autorized_domain);
}

std::shared_ptr<HttpResponse> CreateEmptyResponse() {
  auto response = std::make_shared<HttpResponse>();
  response->headers = std::make_shared<HttpHeaders>();
  return response;
}

TEST(FrontEndServiceV2Test, TestInitFailed) {
  FrontEndServiceV2PeerOptions options;
  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);
  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_FALSE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);
}

TEST(FrontEndServiceV2Test, TestInitSuccess) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();
  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);
  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);
}

TEST(FrontEndServiceV2Test, TestBeginTransaction) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);

  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  execution_result = front_end_service_v2_peer.BeginTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      options.metric_router->GetExportedData();

  // test metric counters
  const std::map<std::string, std::string> begin_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelBeginTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(begin_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      total_request_metric_point_data =
          core::GetMetricPointData(kMetricNameRequests, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      client_error_metric_point_data =
          core::GetMetricPointData(kMetricNameClientErrors, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      server_error_metric_point_data =
          core::GetMetricPointData(kMetricNameServerErrors, dimensions, data);

  ASSERT_TRUE(total_request_metric_point_data.has_value());
  ASSERT_FALSE(client_error_metric_point_data.has_value());
  ASSERT_FALSE(server_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      total_request_metric_point_data.value()));
  auto total_request_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          total_request_metric_point_data.value());

  ASSERT_EQ(std::get<int64_t>(total_request_sum_point_data.value_), 1)
      << "Expected total_request_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_TRUE(execution_result.Successful())
      << core::GetErrorMessage(execution_result.status_code);
  std::shared_ptr<MockAggregateMetric> mock_aggregate_metric =
      front_end_service_v2_peer.GetMetricsInstance(kMetricLabelBeginTransaction,
                                                   kMetricNameRequests);
  EXPECT_THAT(mock_aggregate_metric->GetCounter(kMetricLabelValueOperator), 1);
}

TEST(FrontEndServiceV2Test, TestBeginTransactionWithEmptyHeader) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.response = CreateEmptyResponse();

  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);

  execution_result = front_end_service_v2_peer.BeginTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      options.metric_router->GetExportedData();

  const std::map<std::string, std::string> begin_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelBeginTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(begin_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      total_request_metric_point_data =
          core::GetMetricPointData(kMetricNameRequests, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      client_error_metric_point_data =
          core::GetMetricPointData(kMetricNameClientErrors, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      server_error_metric_point_data =
          core::GetMetricPointData(kMetricNameServerErrors, dimensions, data);

  ASSERT_TRUE(total_request_metric_point_data.has_value());
  ASSERT_TRUE(client_error_metric_point_data.has_value());
  ASSERT_FALSE(server_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      total_request_metric_point_data.value()));
  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      client_error_metric_point_data.value()));

  auto total_request_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          total_request_metric_point_data.value());
  auto client_error_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          client_error_metric_point_data.value());

  ASSERT_EQ(std::get<int64_t>(total_request_sum_point_data.value_), 1)
      << "Expected total_request_sum_point_data.value_ to be 1 (int64_t)";

  ASSERT_EQ(std::get<int64_t>(client_error_sum_point_data.value_), 1)
      << "Expected client_error_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_FALSE(execution_result.Successful())
      << core::GetErrorMessage(execution_result.status_code);

  std::shared_ptr<MockAggregateMetric> mock_aggregate_metric =
      front_end_service_v2_peer.GetMetricsInstance(kMetricLabelBeginTransaction,
                                                   kMetricNameRequests);
  EXPECT_THAT(mock_aggregate_metric->GetCounter(kMetricLabelValueOperator), 1);
}

TEST(FrontEndServiceV2Test, TestBeginTransactionWithConstructorWithLessParams) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  auto mock_config_provider = std::make_shared<MockConfigProvider>();
  static constexpr char kClaimedIdentity[] = "123";
  mock_config_provider->Set(kRemotePrivacyBudgetServiceClaimedIdentity,
                            kClaimedIdentity);

  std::unique_ptr<FrontEndServiceV2> front_end_service_v2 =
      std::make_unique<FrontEndServiceV2>(
          std::make_shared<MockHttpServerInterface>(),
          std::make_shared<MockAsyncExecutor>(),
          std::make_shared<MockMetricClient>(), mock_config_provider,
          budget_consumption_helper.get());
  FrontEndServiceV2Peer front_end_service_v2_peer(
      std::move(front_end_service_v2));

  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);
  EXPECT_TRUE(
      front_end_service_v2_peer.BeginTransaction(http_context).Successful());
}

TEST(FrontEndServiceV2Test, TestBeginTransactionWithoutInit) {
  FrontEndServiceV2PeerOptions options;
  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.response = CreateEmptyResponse();
  EXPECT_THAT(
      front_end_service_v2_peer.BeginTransaction(http_context).status_code,
      SC_PBS_FRONT_END_SERVICE_UNABLE_TO_FIND_TRANSACTION_METRICS);
}

TEST(FrontEndServiceV2Test, TestPrepareTransaction) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes = std::make_shared<std::vector<Byte>>(
      kRequestBody.begin(), kRequestBody.end());
  http_context.request->body.capacity = kRequestBody.length();
  http_context.request->body.length = kRequestBody.length();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  AsyncContext<HttpRequest, HttpResponse> captured_http_context;
  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    captured_http_context = context;
    has_captured = true;
  };

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);
  ASSERT_TRUE(front_end_service_v2_peer.Init());

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper, ConsumeBudgets)
      .WillOnce([&](AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
                        context) {
        context.result = SuccessExecutionResult();
        context.Finish();
        captured_consume_budgets_context = context;
        return SuccessExecutionResult();
      });

  auto execution_result =
      front_end_service_v2_peer.PrepareTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      options.metric_router->GetExportedData();

  const std::map<std::string, std::string> prepare_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelPrepareTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(prepare_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      total_request_metric_point_data =
          core::GetMetricPointData(kMetricNameRequests, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      client_error_metric_point_data =
          core::GetMetricPointData(kMetricNameClientErrors, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      server_error_metric_point_data =
          core::GetMetricPointData(kMetricNameServerErrors, dimensions, data);

  ASSERT_TRUE(total_request_metric_point_data.has_value());
  ASSERT_FALSE(client_error_metric_point_data.has_value());
  ASSERT_FALSE(server_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      total_request_metric_point_data.value()));
  auto total_request_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          total_request_metric_point_data.value());

  ASSERT_EQ(std::get<int64_t>(total_request_sum_point_data.value_), 1)
      << "Expected total_request_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);
  ASSERT_TRUE(has_captured);
  EXPECT_TRUE(captured_http_context.result)
      << core::GetErrorMessage(captured_http_context.result.status_code);
  EXPECT_TRUE(captured_http_context.response->headers->find(
                  kTransactionLastExecutionTimestampHeader) !=
              captured_http_context.response->headers->end());

  ASSERT_EQ(captured_consume_budgets_context.request->budgets.size(), 2);
  EXPECT_EQ(
      *captured_consume_budgets_context.request->budgets[0].budget_key_name,
      "https://fake.com/test_key");
  EXPECT_EQ(captured_consume_budgets_context.request->budgets[0].token_count,
            10);
  EXPECT_EQ(captured_consume_budgets_context.request->budgets[0].time_bucket,
            1570864850000000000);

  EXPECT_EQ(
      *captured_consume_budgets_context.request->budgets[1].budget_key_name,
      "https://fake.com/test_key_2");
  EXPECT_EQ(captured_consume_budgets_context.request->budgets[1].token_count,
            23);
  EXPECT_EQ(captured_consume_budgets_context.request->budgets[1].time_bucket,
            1576135250000000000);

  std::shared_ptr<MockAggregateMetric> mock_aggregate_metric =
      front_end_service_v2_peer.GetMetricsInstance(
          kMetricLabelPrepareTransaction, kMetricNameRequests);
  EXPECT_THAT(mock_aggregate_metric->GetCounter(kMetricLabelValueOperator), 1);
}

TEST(FrontEndServiceV2Test, TestPrepareTransactionBudgetExhausted) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes = std::make_shared<std::vector<Byte>>(
      kRequestBody.begin(), kRequestBody.end());
  http_context.request->body.capacity = kRequestBody.length();
  http_context.request->body.length = kRequestBody.length();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  AsyncContext<HttpRequest, HttpResponse> captured_http_context;
  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    captured_http_context = context;
    has_captured = true;
  };

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);
  ASSERT_TRUE(front_end_service_v2_peer.Init());

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper, ConsumeBudgets)
      .WillOnce([&](AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
                        context) {
        context.result = FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED);
        context.response->budget_exhausted_indices.push_back(0);
        context.Finish();
        captured_consume_budgets_context = context;
        return SuccessExecutionResult();
      });

  auto execution_result =
      front_end_service_v2_peer.PrepareTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      options.metric_router->GetExportedData();

  const std::map<std::string, std::string> prepare_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelPrepareTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(prepare_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      total_request_metric_point_data =
          core::GetMetricPointData(kMetricNameRequests, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      client_error_metric_point_data =
          core::GetMetricPointData(kMetricNameClientErrors, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      server_error_metric_point_data =
          core::GetMetricPointData(kMetricNameServerErrors, dimensions, data);

  ASSERT_TRUE(total_request_metric_point_data.has_value());
  ASSERT_FALSE(client_error_metric_point_data.has_value());
  ASSERT_TRUE(server_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      total_request_metric_point_data.value()));
  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      server_error_metric_point_data.value()));

  auto total_request_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          total_request_metric_point_data.value());
  auto server_error_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          server_error_metric_point_data.value());

  ASSERT_EQ(std::get<int64_t>(total_request_sum_point_data.value_), 1)
      << "Expected total_request_sum_point_data.value_ to be 1 (int64_t)";

  ASSERT_EQ(std::get<int64_t>(server_error_sum_point_data.value_), 1)
      << "Expected server_error_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);
  ASSERT_TRUE(has_captured);
  EXPECT_FALSE(captured_http_context.result);
  EXPECT_EQ(captured_http_context.result.status_code,
            SC_CONSUME_BUDGET_EXHAUSTED);
  EXPECT_TRUE(captured_http_context.response->headers->find(
                  kTransactionLastExecutionTimestampHeader) ==
              captured_http_context.response->headers->end());
  EXPECT_EQ(captured_http_context.response->body.ToString(),
            kBudgetExhaustedResponseBody);

  std::shared_ptr<MockAggregateMetric> mock_aggregate_metric =
      front_end_service_v2_peer.GetMetricsInstance(
          kMetricLabelPrepareTransaction, kMetricNameRequests);
  EXPECT_THAT(mock_aggregate_metric->GetCounter(kMetricLabelValueOperator), 1);
}

TEST(FrontEndServiceV2Test, TestCommitTransaction) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);

  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);

  execution_result = front_end_service_v2_peer.CommitTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      options.metric_router->GetExportedData();

  const std::map<std::string, std::string> commit_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelCommitTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(commit_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      total_request_metric_point_data =
          core::GetMetricPointData(kMetricNameRequests, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      client_error_metric_point_data =
          core::GetMetricPointData(kMetricNameClientErrors, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      server_error_metric_point_data =
          core::GetMetricPointData(kMetricNameServerErrors, dimensions, data);

  ASSERT_TRUE(total_request_metric_point_data.has_value());
  ASSERT_FALSE(client_error_metric_point_data.has_value());
  ASSERT_FALSE(server_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      total_request_metric_point_data.value()));
  auto total_request_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          total_request_metric_point_data.value());

  ASSERT_EQ(std::get<int64_t>(total_request_sum_point_data.value_), 1)
      << "Expected total_request_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_TRUE(execution_result.Successful())
      << core::GetErrorMessage(execution_result.status_code);

  std::shared_ptr<MockAggregateMetric> mock_aggregate_metric =
      front_end_service_v2_peer.GetMetricsInstance(
          kMetricLabelCommitTransaction, kMetricNameRequests);
  EXPECT_THAT(mock_aggregate_metric->GetCounter(kMetricLabelValueOperator), 1);
}

TEST(FrontEndServiceV2Test, TestNotifyTransaction) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);

  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);

  execution_result = front_end_service_v2_peer.NotifyTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      options.metric_router->GetExportedData();

  const std::map<std::string, std::string> notify_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelNotifyTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(notify_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      total_request_metric_point_data =
          core::GetMetricPointData(kMetricNameRequests, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      client_error_metric_point_data =
          core::GetMetricPointData(kMetricNameClientErrors, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      server_error_metric_point_data =
          core::GetMetricPointData(kMetricNameServerErrors, dimensions, data);

  ASSERT_TRUE(total_request_metric_point_data.has_value());
  ASSERT_FALSE(client_error_metric_point_data.has_value());
  ASSERT_FALSE(server_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      total_request_metric_point_data.value()));
  auto total_request_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          total_request_metric_point_data.value());

  ASSERT_EQ(std::get<int64_t>(total_request_sum_point_data.value_), 1)
      << "Expected total_request_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_TRUE(execution_result.Successful())
      << core::GetErrorMessage(execution_result.status_code);
  std::shared_ptr<MockAggregateMetric> mock_aggregate_metric =
      front_end_service_v2_peer.GetMetricsInstance(
          kMetricLabelNotifyTransaction, kMetricNameRequests);
  EXPECT_THAT(mock_aggregate_metric->GetCounter(kMetricLabelValueOperator), 1);
}

TEST(FrontEndServiceV2Test, TestAbortTransaction) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);

  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);

  execution_result = front_end_service_v2_peer.AbortTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      options.metric_router->GetExportedData();

  const std::map<std::string, std::string> abort_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelAbortTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(abort_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      total_request_metric_point_data =
          core::GetMetricPointData(kMetricNameRequests, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      client_error_metric_point_data =
          core::GetMetricPointData(kMetricNameClientErrors, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      server_error_metric_point_data =
          core::GetMetricPointData(kMetricNameServerErrors, dimensions, data);

  ASSERT_TRUE(total_request_metric_point_data.has_value());
  ASSERT_FALSE(client_error_metric_point_data.has_value());
  ASSERT_FALSE(server_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      total_request_metric_point_data.value()));
  auto total_request_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          total_request_metric_point_data.value());

  ASSERT_EQ(std::get<int64_t>(total_request_sum_point_data.value_), 1)
      << "Expected total_request_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_TRUE(execution_result.Successful())
      << core::GetErrorMessage(execution_result.status_code);
  std::shared_ptr<MockAggregateMetric> mock_aggregate_metric =
      front_end_service_v2_peer.GetMetricsInstance(kMetricLabelAbortTransaction,
                                                   kMetricNameRequests);
  EXPECT_THAT(mock_aggregate_metric->GetCounter(kMetricLabelValueOperator), 1);
}

TEST(FrontEndServiceV2Test, TestEndTransaction) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);

  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << core::GetErrorMessage(execution_result.status_code);

  execution_result = front_end_service_v2_peer.EndTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      options.metric_router->GetExportedData();

  const std::map<std::string, std::string> end_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelEndTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(end_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      total_request_metric_point_data =
          core::GetMetricPointData(kMetricNameRequests, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      client_error_metric_point_data =
          core::GetMetricPointData(kMetricNameClientErrors, dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      server_error_metric_point_data =
          core::GetMetricPointData(kMetricNameServerErrors, dimensions, data);

  ASSERT_TRUE(total_request_metric_point_data.has_value());
  ASSERT_FALSE(client_error_metric_point_data.has_value());
  ASSERT_FALSE(server_error_metric_point_data.has_value());

  ASSERT_TRUE(std::holds_alternative<opentelemetry::sdk::metrics::SumPointData>(
      total_request_metric_point_data.value()));
  auto total_request_sum_point_data =
      std::get<opentelemetry::sdk::metrics::SumPointData>(
          total_request_metric_point_data.value());

  ASSERT_EQ(std::get<int64_t>(total_request_sum_point_data.value_), 1)
      << "Expected total_request_sum_point_data.value_ to be 1 (int64_t)";

  EXPECT_TRUE(execution_result.Successful())
      << core::GetErrorMessage(execution_result.status_code);
  std::shared_ptr<MockAggregateMetric> mock_aggregate_metric =
      front_end_service_v2_peer.GetMetricsInstance(kMetricLabelEndTransaction,
                                                   kMetricNameRequests);
  EXPECT_THAT(mock_aggregate_metric->GetCounter(kMetricLabelValueOperator), 1);
}

TEST(FrontEndServiceV2Test, TestRegisterResourceHandlerIsCalled) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  std::shared_ptr<MockHttpServerInterface> http2_server =
      std::make_shared<MockHttpServerInterface>();
  EXPECT_CALL(*http2_server, RegisterResourceHandler(HttpMethod::POST, _, _))
      .Times(8)
      .WillRepeatedly(Return(SuccessExecutionResult()));
  EXPECT_CALL(*http2_server, RegisterResourceHandler(HttpMethod::GET, _, _))
      .Times(1)
      .WillRepeatedly(Return(SuccessExecutionResult()));
  options.http_server = http2_server;
  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);
  EXPECT_TRUE(front_end_service_v2_peer.Init());
}

TEST(FrontEndServiceV2Test, TestGetTransactionStatusReturns404) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      http_context);
  http_context.response = CreateEmptyResponse();

  FrontEndServiceV2Peer front_end_service_v2_peer =
      MakeFrontEndServiceV2Peer(options);
  ASSERT_TRUE(front_end_service_v2_peer.Init());

  EXPECT_THAT(
      front_end_service_v2_peer.GetTransactionStatus(http_context).status_code,
      SC_PBS_FRONT_END_SERVICE_GET_TRANSACTION_STATUS_RETURNS_404_BY_DEFAULT);
}

}  // namespace
}  // namespace google::scp::pbs
