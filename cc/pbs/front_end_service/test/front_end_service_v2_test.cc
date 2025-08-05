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
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "cc/core/async_executor/mock/mock_async_executor.h"
#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/errors.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/telemetry/mock/in_memory_metric_router.h"
#include "cc/core/telemetry/src/common/metric_utils.h"
#include "cc/pbs/consume_budget/src/binary_budget_consumer.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"
#include "google/protobuf/text_format.h"

namespace privacy_sandbox::pbs {

namespace {
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs_common::AsyncContext;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::ExecutionResultOr;
using ::privacy_sandbox::pbs_common::HttpRequest;
using ::privacy_sandbox::pbs_common::HttpResponse;
}  // namespace

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

  ExecutionResultOr<std::unique_ptr<BudgetConsumer>> GetBudgetConsumer(
      const ConsumePrivacyBudgetRequest& req) {
    return front_end_service_v2_->GetBudgetConsumer(req);
  }

 private:
  std::unique_ptr<FrontEndServiceV2> front_end_service_v2_;
};

namespace {

using ::google::protobuf::TextFormat;
using ::privacy_sandbox::pbs_common::AsyncContext;
using ::privacy_sandbox::pbs_common::Byte;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::GetErrorMessage;
using ::privacy_sandbox::pbs_common::GetMetricPointData;
using ::privacy_sandbox::pbs_common::HttpHandler;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::HttpMethod;
using ::privacy_sandbox::pbs_common::HttpServerInterface;
using ::privacy_sandbox::pbs_common::InMemoryMetricRouter;
using ::privacy_sandbox::pbs_common::MockAsyncExecutor;
using ::privacy_sandbox::pbs_common::MockConfigProvider;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;

constexpr absl::string_view kTransactionId =
    "3E2A3D09-48ED-A355-D346-AD7DC6CB0909";
constexpr absl::string_view kTransactionSecret = "secret";
constexpr absl::string_view kReportingOrigin = "https://fake.com";
constexpr absl::string_view kClaimedIdentity = "https://origin.site.com";
constexpr absl::string_view kUserAgent = "aggregation-service/2.8.7";
constexpr size_t k20191212DaysFromEpoch = 18242;
constexpr size_t k20191012DaysFromEpoch = 18181;
constexpr absl::string_view kRequestBody = R"({
        "v": "2.0",
        "data": [
            {
                "reporting_origin": "https://fake.com",
                "keys": [
                    {
                        "key": "test_key",
                        "token": 1,
                        "reporting_time": "2019-10-12T07:20:50.52Z"
                    },
                    {
                        "key": "test_key_2",
                        "token": 1,
                        "reporting_time": "2019-12-12T07:20:50.52Z"
                    }
                ]
            }
        ]
    })";
constexpr absl::string_view kBudgetExhaustedResponseBody =
    R"({"f":[0],"v":"1.0"})";

class MockBudgetConsumptionHelper : public BudgetConsumptionHelperInterface {
 public:
  MOCK_METHOD(ExecutionResult, ConsumeBudgets,
              ((AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
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

struct FrontEndServiceV2PeerOptions {
  BudgetConsumptionHelperInterface* budget_consumption_helper = nullptr;
  InMemoryMetricRouter* metric_router = nullptr;
  std::shared_ptr<MockConfigProvider> mock_config_provider = nullptr;
  std::shared_ptr<MockHttpServerInterface> http2_server = nullptr;
};

std::unique_ptr<FrontEndServiceV2Peer> MakeFrontEndServiceV2Peer(
    FrontEndServiceV2PeerOptions& options) {
  if (options.mock_config_provider == nullptr) {
    options.mock_config_provider = std::make_shared<MockConfigProvider>();
  }

  if (options.http2_server == nullptr) {
    options.http2_server = std::make_shared<MockHttpServerInterface>();
  }

  std::unique_ptr<FrontEndServiceV2> front_end_service_v2 =
      std::make_unique<FrontEndServiceV2>(
          options.http2_server, std::make_shared<MockAsyncExecutor>(),
          options.mock_config_provider, options.budget_consumption_helper,
          options.metric_router);
  return std::make_unique<FrontEndServiceV2Peer>(
      std::move(front_end_service_v2));
}

// Some tests does test the Init() functionality success or
// failure based on different scenarios. Since the SetUp() performs Init(),
// those tests cannout use this test fixture.

class FrontEndServiceV2LifecycleTest : public testing::Test {
 protected:
  void SetUp() override {
    metric_router_ = std::make_unique<InMemoryMetricRouter>();
    budget_consumption_helper_ =
        std::make_unique<MockBudgetConsumptionHelper>();

    FrontEndServiceV2PeerOptions options;
    options.budget_consumption_helper = budget_consumption_helper_.get();
    options.metric_router = metric_router_.get();
    options.mock_config_provider = std::make_shared<MockConfigProvider>();
    options.http2_server = http2_server_;
    front_end_service_v2_peer_ = MakeFrontEndServiceV2Peer(options);

    auto execution_result = front_end_service_v2_peer_->Init();
    EXPECT_TRUE(execution_result)
        << GetErrorMessage(execution_result.status_code);
  }

  std::shared_ptr<MockHttpServerInterface> http2_server_;
  std::unique_ptr<InMemoryMetricRouter> metric_router_;
  std::unique_ptr<MockBudgetConsumptionHelper> budget_consumption_helper_;
  std::unique_ptr<FrontEndServiceV2Peer> front_end_service_v2_peer_;
};

void InsertCommonHeaders(
    absl::string_view transaction_id, absl::string_view secret,
    absl::string_view autorized_domain, absl::string_view claimed_identity,
    absl::string_view user_agent,
    AsyncContext<HttpRequest, HttpResponse>& http_context) {
  http_context.request->headers = std::make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {std::string(kTransactionIdHeader), std::string(transaction_id)});
  http_context.request->auth_context.authorized_domain =
      std::make_shared<std::string>(autorized_domain);
  http_context.request->headers->insert(
      {"x-gscp-claimed-identity", std::string(claimed_identity)});
  http_context.request->headers->insert(
      {"user-agent", std::string(user_agent)});
}

std::shared_ptr<HttpResponse> CreateEmptyResponse() {
  auto response = std::make_shared<HttpResponse>();
  response->headers = std::make_shared<HttpHeaders>();
  return response;
}

TEST(FrontEndServiceV2Test, TestInitFailed) {
  FrontEndServiceV2PeerOptions options;
  auto front_end_service_v2_peer = MakeFrontEndServiceV2Peer(options);
  auto execution_result = front_end_service_v2_peer->Init();
  EXPECT_FALSE(execution_result)
      << GetErrorMessage(execution_result.status_code);
}

TEST(FrontEndServiceV2Test, TestInitSuccess) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();
  auto front_end_service_v2_peer = MakeFrontEndServiceV2Peer(options);
  auto execution_result = front_end_service_v2_peer->Init();
  EXPECT_TRUE(execution_result)
      << GetErrorMessage(execution_result.status_code);
}

TEST_F(FrontEndServiceV2LifecycleTest, TestBeginTransaction) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  auto execution_result =
      front_end_service_v2_peer_->BeginTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  // test metric counters
  const std::map<std::string, std::string> begin_transaction_label_kv = {
      {"transaction_phase", "BEGIN"},
      {"reporting_origin", "OPERATOR"},
  };

  EXPECT_TRUE(execution_result.Successful())
      << GetErrorMessage(execution_result.status_code);
}

TEST_F(FrontEndServiceV2LifecycleTest, TestBeginTransactionWithEmptyHeader) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.response = CreateEmptyResponse();

  auto execution_result =
      front_end_service_v2_peer_->BeginTransaction(http_context);

  EXPECT_FALSE(execution_result.Successful())
      << GetErrorMessage(execution_result.status_code);
}

TEST(FrontEndServiceV2Test, TestBeginTransactionWithConstructorWithLessParams) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  auto mock_config_provider = std::make_shared<MockConfigProvider>();

  std::unique_ptr<FrontEndServiceV2> front_end_service_v2 =
      std::make_unique<FrontEndServiceV2>(
          std::make_shared<MockHttpServerInterface>(),
          std::make_shared<MockAsyncExecutor>(), mock_config_provider,
          budget_consumption_helper.get());
  FrontEndServiceV2Peer front_end_service_v2_peer(
      std::move(front_end_service_v2));

  auto execution_result = front_end_service_v2_peer.Init();
  EXPECT_TRUE(execution_result)
      << GetErrorMessage(execution_result.status_code);
  EXPECT_TRUE(
      front_end_service_v2_peer.BeginTransaction(http_context).Successful());
}

TEST(FrontEndServiceV2Test, TestBeginTransactionWithoutInit) {
  FrontEndServiceV2PeerOptions options;
  auto front_end_service_v2_peer = MakeFrontEndServiceV2Peer(options);
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.response = CreateEmptyResponse();
  EXPECT_THAT(
      front_end_service_v2_peer->BeginTransaction(http_context).status_code,
      SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
}

TEST_F(FrontEndServiceV2LifecycleTest, TestPrepareTransaction) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes = std::make_shared<std::vector<Byte>>(
      kRequestBody.begin(), kRequestBody.end());
  http_context.request->body.capacity = kRequestBody.length();
  http_context.request->body.length = kRequestBody.length();

  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  AsyncContext<HttpRequest, HttpResponse> captured_http_context;
  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    captured_http_context = context;
    has_captured = true;
  };

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper_, ConsumeBudgets)
      .WillOnce([&](AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
                        context) {
        context.result = SuccessExecutionResult();
        context.response->budget_exhausted_indices.push_back(0);
        context.Finish();
        captured_consume_budgets_context = context;
        return SuccessExecutionResult();
      });

  auto execution_result =
      front_end_service_v2_peer_->PrepareTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> prepare_transaction_label_kv = {
      {"transaction_phase", "PREPARE"},
      {"reporting_origin", "OPERATOR"},
      {"pbs.claimed_identity", "https://origin.site.com"},
      {"scp.http.request.client_version", "aggregation-service/2.8.7"}};

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(prepare_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      keys_per_transaction_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.keys_per_transaction", dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      successful_budget_consumed_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.successful_budget_consumed", dimensions,
          data);

  ASSERT_TRUE(keys_per_transaction_metric_point_data.has_value());
  ASSERT_TRUE(successful_budget_consumed_metric_point_data.has_value());

  // 2 keys/budgets in this transaction.
  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          keys_per_transaction_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData
      keys_per_transaction_histogram_data = reinterpret_cast<
          const opentelemetry::sdk::metrics::HistogramPointData&>(
          keys_per_transaction_metric_point_data.value());

  auto keys_per_transaction_histogram_data_max =
      std::get_if<int64_t>(&keys_per_transaction_histogram_data.max_);
  EXPECT_EQ(*keys_per_transaction_histogram_data_max, 2);

  std::vector<double> keys_boundaries = {
      1.0,    1.5,    2.3,    3.4,    5.1,    7.6,     11.4,    17.1,   25.6,
      38.4,   57.7,   86.5,   129.7,  194.6,  291.9,   437.9,   656.8,  985.3,
      1477.9, 2216.8, 3325.3, 4987.9, 7481.8, 11222.7, 16864.1, 25251.2};

  const std::vector<double>& keys_per_transaction_boundaries =
      keys_per_transaction_histogram_data.boundaries_;

  // Check if the boundaries vector matches the expected boundaries.
  ASSERT_EQ(keys_per_transaction_boundaries.size(), keys_boundaries.size())
      << "Boundaries vector size mismatch.";

  for (size_t i = 0; i < keys_per_transaction_boundaries.size(); ++i) {
    EXPECT_DOUBLE_EQ(keys_per_transaction_boundaries[i], keys_boundaries[i])
        << "Mismatch at index " << i;
  }

  // Successful 2 budgets consumed in this transaction.
  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          successful_budget_consumed_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData
      successful_budget_consumed_histogram_data = reinterpret_cast<
          const opentelemetry::sdk::metrics::HistogramPointData&>(
          successful_budget_consumed_metric_point_data.value());

  auto successful_budget_consumed_histogram_data_max =
      std::get_if<int64_t>(&successful_budget_consumed_histogram_data.max_);
  EXPECT_EQ(*successful_budget_consumed_histogram_data_max, 2);

  const std::vector<double>& successful_budget_consumed_boundaries =
      successful_budget_consumed_histogram_data.boundaries_;

  // Check if the boundaries vector matches the expected boundaries.
  ASSERT_EQ(successful_budget_consumed_boundaries.size(),
            keys_boundaries.size())
      << "Boundaries vector size mismatch.";

  for (size_t i = 0; i < successful_budget_consumed_boundaries.size(); ++i) {
    EXPECT_DOUBLE_EQ(successful_budget_consumed_boundaries[i],
                     keys_boundaries[i])
        << "Mismatch at index " << i;
  }

  EXPECT_TRUE(execution_result)
      << GetErrorMessage(execution_result.status_code);
  ASSERT_TRUE(has_captured);
  EXPECT_TRUE(captured_http_context.result)
      << GetErrorMessage(captured_http_context.result.status_code);

  ASSERT_EQ(
      captured_consume_budgets_context.request->budget_consumer->GetKeyCount(),
      2);
  std::vector<std::string> expected_keys_list{
      absl::StrCat("Budget Key: https://fake.com/test_key", " Day ",
                   std::to_string(k20191012DaysFromEpoch), " Hour 7"),
      absl::StrCat("Budget Key: https://fake.com/test_key_2", " Day ",
                   std::to_string(k20191212DaysFromEpoch), " Hour 7")};
  EXPECT_THAT(
      captured_consume_budgets_context.request->budget_consumer->DebugKeyList(),
      UnorderedElementsAreArray(expected_keys_list));
  // With budget consumer, we serialize budget exhausted indices even for
  // success
  EXPECT_EQ(
      nlohmann::json::parse(captured_http_context.response->body.ToString()),
      nlohmann::json::parse(kBudgetExhaustedResponseBody));
}

TEST_F(FrontEndServiceV2LifecycleTest, TestPrepareTransactionBudgetExhausted) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes = std::make_shared<std::vector<Byte>>(
      kRequestBody.begin(), kRequestBody.end());
  http_context.request->body.capacity = kRequestBody.length();
  http_context.request->body.length = kRequestBody.length();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  AsyncContext<HttpRequest, HttpResponse> captured_http_context;
  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    captured_http_context = context;
    has_captured = true;
  };

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper_, ConsumeBudgets)
      .WillOnce([&](AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
                        context) {
        context.result = FailureExecutionResult(SC_CONSUME_BUDGET_EXHAUSTED);
        context.response->budget_exhausted_indices.push_back(0);
        context.Finish();
        captured_consume_budgets_context = context;
        return SuccessExecutionResult();
      });

  auto execution_result =
      front_end_service_v2_peer_->PrepareTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> prepare_transaction_label_kv = {
      {"transaction_phase", "PREPARE"},
      {"reporting_origin", "OPERATOR"},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(prepare_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      keys_per_transaction_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.keys_per_transaction", dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      successful_budget_consumed_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.successful_budget_consumed", dimensions,
          data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      budget_exhausted_metric_point_data = GetMetricPointData(
          "google.scp.pbs.consume_budget.budget_exhausted", dimensions, data);

  ASSERT_TRUE(keys_per_transaction_metric_point_data.has_value());
  ASSERT_FALSE(successful_budget_consumed_metric_point_data.has_value());
  ASSERT_TRUE(budget_exhausted_metric_point_data.has_value());

  // 2 keys/budgets in this transaction.
  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          keys_per_transaction_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData
      keys_per_transaction_histogram_data = reinterpret_cast<
          const opentelemetry::sdk::metrics::HistogramPointData&>(
          keys_per_transaction_metric_point_data.value());

  // 2 budgets exhausted in this transaction.
  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          budget_exhausted_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData
      budget_exhausted_histogram_data = reinterpret_cast<
          const opentelemetry::sdk::metrics::HistogramPointData&>(
          budget_exhausted_metric_point_data.value());

  std::vector<double> boundaries = {1.0,  2.0,   4.0,   8.0,   16.0,   32.0,
                                    64.0, 128.0, 256.0, 512.0, 1024.0, 2048.0};

  const std::vector<double>& budget_exhausted_boundaries =
      budget_exhausted_histogram_data.boundaries_;

  // Check if the boundaries vector matches the expected boundaries.
  ASSERT_EQ(budget_exhausted_boundaries.size(), boundaries.size())
      << "Boundaries vector size mismatch.";

  for (size_t i = 0; i < budget_exhausted_boundaries.size(); ++i) {
    EXPECT_DOUBLE_EQ(budget_exhausted_boundaries[i], boundaries[i])
        << "Mismatch at index " << i;
  }

  EXPECT_TRUE(execution_result)
      << GetErrorMessage(execution_result.status_code);
  ASSERT_TRUE(has_captured);
  EXPECT_FALSE(captured_http_context.result);
  EXPECT_EQ(captured_http_context.result.status_code,
            SC_CONSUME_BUDGET_EXHAUSTED);
  EXPECT_EQ(
      nlohmann::json::parse(captured_http_context.response->body.ToString()),
      nlohmann::json::parse(kBudgetExhaustedResponseBody));
}

TEST_F(FrontEndServiceV2LifecycleTest,
       TestPrepareTransactionBudgetsNotConsumed) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes = std::make_shared<std::vector<Byte>>(
      kRequestBody.begin(), kRequestBody.end());
  http_context.request->body.capacity = kRequestBody.length();
  http_context.request->body.length = kRequestBody.length();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  AsyncContext<HttpRequest, HttpResponse> captured_http_context;
  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    captured_http_context = context;
    has_captured = true;
  };

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper_, ConsumeBudgets)
      .WillOnce([&](AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
                        context) {
        context.result =
            FailureExecutionResult(SC_CONSUME_BUDGET_FAIL_TO_COMMIT);
        context.response->budget_exhausted_indices.push_back(0);
        context.Finish();
        captured_consume_budgets_context = context;
        return SuccessExecutionResult();
      });

  auto execution_result =
      front_end_service_v2_peer_->PrepareTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> prepare_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelPrepareTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(prepare_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      keys_per_transaction_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.keys_per_transaction", dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      successful_budget_consumed_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.successful_budget_consumed", dimensions,
          data);

  ASSERT_TRUE(keys_per_transaction_metric_point_data.has_value());
  ASSERT_FALSE(successful_budget_consumed_metric_point_data.has_value());

  // 2 keys/budgets in this transaction.
  EXPECT_TRUE(
      std::holds_alternative<opentelemetry::sdk::metrics::HistogramPointData>(
          keys_per_transaction_metric_point_data.value()));

  opentelemetry::sdk::metrics::HistogramPointData
      keys_per_transaction_histogram_data = reinterpret_cast<
          const opentelemetry::sdk::metrics::HistogramPointData&>(
          keys_per_transaction_metric_point_data.value());

  EXPECT_TRUE(execution_result)
      << GetErrorMessage(execution_result.status_code);
  ASSERT_TRUE(has_captured);
  EXPECT_FALSE(captured_http_context.result);
  EXPECT_EQ(captured_http_context.result.status_code,
            SC_CONSUME_BUDGET_FAIL_TO_COMMIT);
}

TEST_F(FrontEndServiceV2LifecycleTest,
       TestPrepareTransactionBudgetConsumerInvalidJson) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes = std::make_shared<std::vector<Byte>>();
  http_context.request->body.capacity = 0;
  http_context.request->body.length = 0;
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    has_captured = true;
  };

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper_, ConsumeBudgets).Times(0);

  auto execution_result =
      front_end_service_v2_peer_->PrepareTransaction(http_context);

  EXPECT_EQ(execution_result.status_code,
            SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  EXPECT_FALSE(has_captured);
}

TEST_F(FrontEndServiceV2LifecycleTest,
       TestPrepareTransactionBudgetConsumerWithEmptyData) {
  constexpr absl::string_view empty_data_json = R"({ "v": "2.0", "data": [] })";

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes = std::make_shared<std::vector<Byte>>(
      empty_data_json.begin(), empty_data_json.end());
  http_context.request->body.capacity = empty_data_json.size();
  http_context.request->body.length = empty_data_json.size();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    has_captured = true;
  };

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper_, ConsumeBudgets).Times(0);

  auto execution_result =
      front_end_service_v2_peer_->PrepareTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> prepare_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelPrepareTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(prepare_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      keys_per_transaction_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.keys_per_transaction", dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      successful_budget_consumed_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.successful_budget_consumed", dimensions,
          data);

  EXPECT_TRUE(keys_per_transaction_metric_point_data.has_value());
  EXPECT_FALSE(successful_budget_consumed_metric_point_data.has_value());

  EXPECT_EQ(execution_result.status_code,
            SC_PBS_FRONT_END_SERVICE_NO_KEYS_AVAILABLE);
  EXPECT_FALSE(has_captured);
}

TEST_F(FrontEndServiceV2LifecycleTest,
       TestPrepareTransactionBudgetConsumerWithEmptyKey) {
  constexpr absl::string_view empty_key_json = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "https://fake.com",
      "keys": []
    }
  ]
})";

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes = std::make_shared<std::vector<Byte>>(
      empty_key_json.begin(), empty_key_json.end());
  http_context.request->body.capacity = empty_key_json.size();
  http_context.request->body.length = empty_key_json.size();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    has_captured = true;
  };

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper_, ConsumeBudgets).Times(0);

  auto execution_result =
      front_end_service_v2_peer_->PrepareTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> prepare_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelPrepareTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(prepare_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      keys_per_transaction_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.keys_per_transaction", dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      successful_budget_consumed_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.successful_budget_consumed", dimensions,
          data);

  EXPECT_TRUE(keys_per_transaction_metric_point_data.has_value());
  EXPECT_FALSE(successful_budget_consumed_metric_point_data.has_value());

  EXPECT_EQ(execution_result.status_code,
            SC_PBS_FRONT_END_SERVICE_NO_KEYS_AVAILABLE);
  EXPECT_FALSE(has_captured);
}

TEST_F(FrontEndServiceV2LifecycleTest,
       TestPrepareTransactionBudgetConsumerUnsupportedBudgetType) {
  constexpr absl::string_view json_body = R"({
  "v": "2.0",
  "data": [
    {
      "reporting_origin": "https://fake.com",
      "keys": [
        {
          "key": "123",
          "token": 1,
          "reporting_time": "2019-12-11T07:20:50.52Z",
          "budget_type": "iamnotsupported"
        }
      ]
    }
  ]
})";

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body.bytes =
      std::make_shared<std::vector<Byte>>(json_body.begin(), json_body.end());
  http_context.request->body.capacity = json_body.size();
  http_context.request->body.length = json_body.size();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  bool has_captured = false;
  http_context.callback = [&](AsyncContext<HttpRequest, HttpResponse> context) {
    has_captured = true;
  };

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      captured_consume_budgets_context;
  EXPECT_CALL(*budget_consumption_helper_, ConsumeBudgets).Times(0);

  auto execution_result =
      front_end_service_v2_peer_->PrepareTransaction(http_context);

  std::vector<opentelemetry::sdk::metrics::ResourceMetrics> data =
      metric_router_->GetExportedData();

  const std::map<std::string, std::string> prepare_transaction_label_kv = {
      {kMetricLabelTransactionPhase, kMetricLabelPrepareTransaction},
      {kMetricLabelKeyReportingOrigin, kMetricLabelValueOperator},
  };

  const opentelemetry::sdk::common::OrderedAttributeMap dimensions(
      (opentelemetry::common::KeyValueIterableView<
          std::map<std::string, std::string>>(prepare_transaction_label_kv)));

  std::optional<opentelemetry::sdk::metrics::PointType>
      keys_per_transaction_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.keys_per_transaction", dimensions, data);
  std::optional<opentelemetry::sdk::metrics::PointType>
      successful_budget_consumed_metric_point_data = GetMetricPointData(
          "google.scp.pbs.frontend.successful_budget_consumed", dimensions,
          data);

  EXPECT_FALSE(keys_per_transaction_metric_point_data.has_value());
  EXPECT_FALSE(successful_budget_consumed_metric_point_data.has_value());

  EXPECT_EQ(execution_result.status_code,
            SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  EXPECT_FALSE(has_captured);
}

TEST_F(FrontEndServiceV2LifecycleTest, TestCommitTransaction) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  auto execution_result =
      front_end_service_v2_peer_->CommitTransaction(http_context);

  EXPECT_TRUE(execution_result.Successful())
      << GetErrorMessage(execution_result.status_code);
}

TEST_F(FrontEndServiceV2LifecycleTest, TestNotifyTransaction) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  auto execution_result =
      front_end_service_v2_peer_->NotifyTransaction(http_context);

  EXPECT_TRUE(execution_result.Successful())
      << GetErrorMessage(execution_result.status_code);
}

TEST_F(FrontEndServiceV2LifecycleTest, TestAbortTransaction) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  auto execution_result =
      front_end_service_v2_peer_->AbortTransaction(http_context);

  EXPECT_TRUE(execution_result.Successful())
      << GetErrorMessage(execution_result.status_code);
}

TEST_F(FrontEndServiceV2LifecycleTest, TestEndTransaction) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  auto execution_result =
      front_end_service_v2_peer_->EndTransaction(http_context);

  EXPECT_TRUE(execution_result.Successful())
      << GetErrorMessage(execution_result.status_code);
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
  options.http2_server = http2_server;
  auto front_end_service_v2_peer = MakeFrontEndServiceV2Peer(options);
  EXPECT_TRUE(front_end_service_v2_peer->Init());
}

TEST_F(FrontEndServiceV2LifecycleTest, TestGetTransactionStatusReturns404) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  InsertCommonHeaders(kTransactionId, kTransactionSecret, kReportingOrigin,
                      kClaimedIdentity, kUserAgent, http_context);
  http_context.response = CreateEmptyResponse();

  EXPECT_THAT(
      front_end_service_v2_peer_->GetTransactionStatus(http_context)
          .status_code,
      SC_PBS_FRONT_END_SERVICE_GET_TRANSACTION_STATUS_RETURNS_404_BY_DEFAULT);
}

TEST(TestOneToOneMappingBetBudgetTypeAndBudgetConsumer,
     TestOneToOneMappingBetBudgetTypeAndBudgetConsumer) {
  auto budget_consumption_helper =
      std::make_unique<MockBudgetConsumptionHelper>();
  FrontEndServiceV2PeerOptions options;
  options.budget_consumption_helper = budget_consumption_helper.get();
  auto front_end_service_v2_peer = MakeFrontEndServiceV2Peer(options);
  auto execution_result = front_end_service_v2_peer->Init();
  ASSERT_TRUE(execution_result)
      << GetErrorMessage(execution_result.status_code);

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  std::string proto_string = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { budget_type: BUDGET_TYPE_BINARY_BUDGET }
    }
  )pb";

  ConsumePrivacyBudgetRequest req;
  ASSERT_TRUE(TextFormat::ParseFromString(proto_string, &req));

  absl::flat_hash_set<std::string> returned_consumer_types;
  const google::protobuf::EnumDescriptor* enum_descriptor =
      PrivacyBudgetKey::BudgetType_descriptor();

  // Do not test BUDGET_TYPE_UNSPECIFIED
  for (int i = 1; i < enum_descriptor->value_count(); ++i) {
    const google::protobuf::EnumValueDescriptor* value_descriptor =
        enum_descriptor->value(i);
    PrivacyBudgetKey::BudgetType budget_type =
        static_cast<PrivacyBudgetKey::BudgetType>(value_descriptor->number());

    req.mutable_data()->at(0).mutable_keys()->at(0).set_budget_type(
        budget_type);
    auto budget_consumer = front_end_service_v2_peer->GetBudgetConsumer(req);
    ASSERT_TRUE(budget_consumer.has_value())
        << GetErrorMessage(budget_consumer.result().status_code);
    ASSERT_NE(budget_consumer->get(), nullptr);

    // Ignore warning about deferencing the pointer
#pragma GCC diagnostic ignored "-Wpotentially-evaluated-expression"
    std::string type_name = typeid(*budget_consumer->get()).name();

    ASSERT_TRUE(returned_consumer_types.insert(type_name).second)
        << "Duplicate consumer type returned for enum value " << budget_type
        << ". Type: " << type_name << " was already returned by another enum.";
  }
}

TEST_F(FrontEndServiceV2LifecycleTest,
       GetBudgetConsumerWithBudgetTypeUnspecifiedReturnsBinaryBudget) {
  std::string request_proto_string = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { budget_type: BUDGET_TYPE_UNSPECIFIED }
    }
  )pb";

  ConsumePrivacyBudgetRequest consume_budget_request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_proto_string,
                                          &consume_budget_request));

  auto budget_consumer =
      front_end_service_v2_peer_->GetBudgetConsumer(consume_budget_request);
  EXPECT_THAT(budget_consumer.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_NE(dynamic_cast<BinaryBudgetConsumer*>(budget_consumer.value().get()),
            nullptr);
}

TEST_F(FrontEndServiceV2LifecycleTest,
       GetBudgetConsumerWithBinaryBudgetTypeReturnsBinaryBudget) {
  std::string request_proto_string = R"pb(
    version: "2.0"
    data {
      reporting_origin: "http://a.fake.com"
      keys { budget_type: BUDGET_TYPE_BINARY_BUDGET }
    }
  )pb";

  ConsumePrivacyBudgetRequest consume_budget_request;
  ASSERT_TRUE(TextFormat::ParseFromString(request_proto_string,
                                          &consume_budget_request));

  auto budget_consumer =
      front_end_service_v2_peer_->GetBudgetConsumer(consume_budget_request);
  EXPECT_THAT(budget_consumer.result(), ResultIs(SuccessExecutionResult()));
  EXPECT_NE(dynamic_cast<BinaryBudgetConsumer*>(budget_consumer.value().get()),
            nullptr);
}

}  // namespace
}  // namespace privacy_sandbox::pbs
