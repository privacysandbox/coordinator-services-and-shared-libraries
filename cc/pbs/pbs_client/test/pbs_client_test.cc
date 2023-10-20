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

#include "pbs/pbs_client/src/pbs_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/authorization_service/src/error_codes.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/authorization_service_interface.h"
#include "core/token_provider_cache/mock/token_provider_cache_mock.h"
#include "pbs/front_end_service/src/error_codes.h"
#include "pbs/pbs_client/mock/mock_pbs_client_with_overrides.h"
#include "pbs/pbs_client/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncOperation;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::ResultIs;
using google::scp::core::token_provider_cache::mock::MockTokenProviderCache;
using google::scp::pbs::PrivacyBudgetServiceClient;
using google::scp::pbs::client::mock::
    MockPrivacyBudgetServiceClientWithOverrides;
using std::function;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using testing::Return;

namespace google::scp::pbs::test {

class PBSClientTest : public testing::Test {
 protected:
  PBSClientTest()
      : pbs_endpoint_("http://www.pbs_endpoint.com"),
        reporting_origin_("ads-google.com"),
        mock_http_client_(make_shared<MockHttpClient>()),
        http_client_(mock_http_client_),
        mock_async_executor_(make_shared<MockAsyncExecutor>()),
        mock_auth_token_provider_cache_(make_shared<MockTokenProviderCache>()),
        auth_token_provider_cache_(mock_auth_token_provider_cache_) {
    ON_CALL(*mock_auth_token_provider_cache_, GetToken)
        .WillByDefault(Return(make_shared<string>("dummy_token")));
  }

  void ExecuteTransactionPhaseHelper(TransactionExecutionPhase phase,
                                     string phase_str);

  shared_ptr<TransactionPhaseRequest> GetSampleTransactionPhaseRequest() {
    auto request = make_shared<TransactionPhaseRequest>();
    request->transaction_id = Uuid::GenerateUuid();
    request->transaction_secret = make_shared<string>("This is secret");
    request->transaction_origin =
        make_shared<string>("This is transaction origin");
    request->transaction_execution_phase = TransactionExecutionPhase::Commit;
    request->last_execution_timestamp = 1234567890;
    return request;
  }

  shared_ptr<ConsumeBudgetTransactionRequest>
  GetSampleConsumeBudgetTransactionRequest() {
    auto request = make_shared<ConsumeBudgetTransactionRequest>();
    request->transaction_id = Uuid::GenerateUuid();
    request->transaction_secret = make_shared<string>("This is secret");
    ConsumeBudgetMetadata consume_metadata = {
        make_shared<string>("test_budget_key"), TokenCount(12345), 1};
    request->budget_keys = make_shared<vector<ConsumeBudgetMetadata>>(
        vector<ConsumeBudgetMetadata>{consume_metadata});
    return request;
  }

  shared_ptr<GetTransactionStatusRequest>
  GetSampleGetTransactionStatusRequest() {
    auto request = make_shared<GetTransactionStatusRequest>();
    request->transaction_id = Uuid::GenerateUuid();
    request->transaction_secret = make_shared<string>("This is secret");
    request->transaction_origin =
        make_shared<string>("This is transaction origin");
    return request;
  }

  string pbs_endpoint_;
  string reporting_origin_;
  std::shared_ptr<MockHttpClient> mock_http_client_;
  std::shared_ptr<HttpClientInterface> http_client_;
  std::shared_ptr<MockAsyncExecutor> mock_async_executor_;
  std::shared_ptr<MockTokenProviderCache> mock_auth_token_provider_cache_;
  std::shared_ptr<TokenProviderCacheInterface> auth_token_provider_cache_;
};

TEST_F(PBSClientTest, ValidateEndpointUrls) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  EXPECT_EQ(privacy_budget_service_client.GetExecuteTransactionBeginPhaseUrl(),
            "http://www.pbs_endpoint.com/v1/transactions:begin");
  EXPECT_EQ(
      privacy_budget_service_client.GetExecuteTransactionPreparePhaseUrl(),
      "http://www.pbs_endpoint.com/v1/transactions:prepare");
  EXPECT_EQ(privacy_budget_service_client.GetExecuteTransactionCommitPhaseUrl(),
            "http://www.pbs_endpoint.com/v1/transactions:commit");
  EXPECT_EQ(privacy_budget_service_client.GetExecuteTransactionNotifyPhaseUrl(),
            "http://www.pbs_endpoint.com/v1/transactions:notify");
  EXPECT_EQ(privacy_budget_service_client.GetExecuteTransactionAbortPhaseUrl(),
            "http://www.pbs_endpoint.com/v1/transactions:abort");
  EXPECT_EQ(privacy_budget_service_client.GetExecuteTransactionEndPhaseUrl(),
            "http://www.pbs_endpoint.com/v1/transactions:end");
  EXPECT_EQ(privacy_budget_service_client.GetTransactionStatusUrl(),
            "http://www.pbs_endpoint.com/v1/transactions:status");
}

TEST_F(PBSClientTest, InitiateConsumeBudgetTransactionHttpClientFailures) {
  PrivacyBudgetServiceClient privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context.request =
      make_shared<ConsumeBudgetTransactionRequest>();

  consume_budget_transaction_context.request->transaction_id =
      Uuid::GenerateUuid();
  consume_budget_transaction_context.request->transaction_secret =
      make_shared<string>("This is secret");

  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);

  metadata.budget_key_name = make_shared<string>("test_key");
  metadata.time_bucket = 23;
  metadata.token_count = 2;
  budget_keys->push_back(metadata);
  consume_budget_transaction_context.request->budget_keys = budget_keys;

  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    bool is_called = false;
    mock_http_client_->perform_request_mock =
        [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
          is_called = true;
          return result;
        };

    EXPECT_EQ(privacy_budget_service_client.InitiateConsumeBudgetTransaction(
                  consume_budget_transaction_context),
              result);

    EXPECT_EQ(is_called, true);
  }
}

TEST_F(PBSClientTest, InitiateConsumeBudgetTransaction) {
  PrivacyBudgetServiceClient privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  mock_async_executor_->schedule_for_mock =
      [](const core::AsyncOperation& work, core::Timestamp,
         std::function<bool()>&) { return SuccessExecutionResult(); };
  privacy_budget_service_client.Init();
  privacy_budget_service_client.Run();

  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context.request =
      make_shared<ConsumeBudgetTransactionRequest>();

  consume_budget_transaction_context.request->transaction_id =
      Uuid::GenerateUuid();
  consume_budget_transaction_context.request->transaction_secret =
      make_shared<string>("This is secret");

  EXPECT_EQ(privacy_budget_service_client.InitiateConsumeBudgetTransaction(
                consume_budget_transaction_context),
            FailureExecutionResult(
                core::errors::SC_PBS_CLIENT_NO_BUDGET_KEY_PROVIDED));

  auto budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 1576135250000000000;
  metadata.token_count = 1;
  budget_keys->push_back(metadata);

  metadata.budget_key_name = make_shared<string>("test_key");
  metadata.time_bucket = 1686135250000000000;
  metadata.token_count = 2;
  budget_keys->push_back(metadata);
  consume_budget_transaction_context.request->budget_keys = budget_keys;

  bool is_called = false;
  mock_http_client_->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        EXPECT_EQ(*http_context.request->path,
                  "http://www.pbs_endpoint.com/v1/transactions:begin");
        EXPECT_EQ(http_context.request->method, HttpMethod::POST);
        EXPECT_NE(http_context.request->headers->find(string(core::kAuthHeader))
                      ->second.size(),
                  0);
        EXPECT_EQ(http_context.request->headers
                      ->find(string(core::kClaimedIdentityHeader))
                      ->second,
                  reporting_origin_);
        EXPECT_EQ(
            http_context.request->headers->find(string(kTransactionIdHeader))
                ->second,
            ToString(
                consume_budget_transaction_context.request->transaction_id));
        EXPECT_EQ(
            http_context.request->headers
                ->find(string(kTransactionSecretHeader))
                ->second,
            *consume_budget_transaction_context.request->transaction_secret);

        string body(http_context.request->body.bytes->begin(),
                    http_context.request->body.bytes->end());
        EXPECT_EQ(body,
                  "{\"t\":[{\"key\":\"test_budget_key\",\"reporting_time\":"
                  "\"2019-12-12T07:20:50Z\",\"token\":1},{\"key\":\"test_key\","
                  "\"reporting_time\":"
                  "\"2023-06-07T10:54:10Z\",\"token\":2}],\"v\":\"1.0\"}");
        is_called = true;
        return SuccessExecutionResult();
      };

  EXPECT_EQ(privacy_budget_service_client.InitiateConsumeBudgetTransaction(
                consume_budget_transaction_context),
            SuccessExecutionResult());

  EXPECT_EQ(is_called, true);
}

TEST_F(PBSClientTest, OnInitiateConsumeBudgetTransactionCallbackHttpFailure) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    bool is_called = false;
    http_context.result = result;
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>
        consume_budget_transaction_context;
    consume_budget_transaction_context.callback =
        [&](AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
                consume_budget_transaction_context) {
          EXPECT_THAT(consume_budget_transaction_context.result,
                      ResultIs(result));
          is_called = true;
        };

    privacy_budget_service_client.OnInitiateConsumeBudgetTransactionCallback(
        consume_budget_transaction_context, http_context);

    EXPECT_EQ(is_called, true);
  }
}

TEST_F(PBSClientTest, OnInitiateConsumeBudgetTransactionCallbackHttpNoHeader) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  http_context.response->headers = make_shared<HttpHeaders>();

  bool is_called = false;
  http_context.result = SuccessExecutionResult();
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context.callback =
      [&](AsyncContext<ConsumeBudgetTransactionRequest,
                       ConsumeBudgetTransactionResponse>&
              consume_budget_transaction_context) {
        EXPECT_THAT(
            consume_budget_transaction_context.result,
            ResultIs(FailureExecutionResult(
                core::errors::SC_PBS_CLIENT_RESPONSE_HEADER_NOT_FOUND)));
        is_called = true;
      };

  privacy_budget_service_client.OnInitiateConsumeBudgetTransactionCallback(
      consume_budget_transaction_context, http_context);

  EXPECT_EQ(is_called, true);

  is_called = false;
  http_context.result = SuccessExecutionResult();
  http_context.response->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader), "dasdas"});

  consume_budget_transaction_context.callback =
      [&](AsyncContext<ConsumeBudgetTransactionRequest,
                       ConsumeBudgetTransactionResponse>&
              consume_budget_transaction_context) {
        EXPECT_EQ(consume_budget_transaction_context.result,
                  FailureExecutionResult(
                      core::errors::SC_PBS_CLIENT_INVALID_RESPONSE_HEADER));
        is_called = true;
      };

  privacy_budget_service_client.OnInitiateConsumeBudgetTransactionCallback(
      consume_budget_transaction_context, http_context);

  EXPECT_EQ(is_called, true);

  is_called = false;
  http_context.result = SuccessExecutionResult();
  http_context.response->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader),
       "123456789012345678901"});

  consume_budget_transaction_context.callback =
      [&](AsyncContext<ConsumeBudgetTransactionRequest,
                       ConsumeBudgetTransactionResponse>&
              consume_budget_transaction_context) {
        EXPECT_EQ(consume_budget_transaction_context.result,
                  FailureExecutionResult(
                      core::errors::SC_PBS_CLIENT_INVALID_RESPONSE_HEADER));
        is_called = true;
      };

  privacy_budget_service_client.OnInitiateConsumeBudgetTransactionCallback(
      consume_budget_transaction_context, http_context);

  EXPECT_EQ(is_called, true);
}

TEST_F(PBSClientTest, OnInitiateConsumeBudgetTransactionCallback) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  http_context.response->headers = make_shared<HttpHeaders>();

  bool is_called = false;

  http_context.result = SuccessExecutionResult();
  http_context.response->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader),
       "1234567890123456789"});

  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context
      .callback = [&](AsyncContext<ConsumeBudgetTransactionRequest,
                                   ConsumeBudgetTransactionResponse>&
                          consume_budget_transaction_context) {
    EXPECT_EQ(consume_budget_transaction_context.result,
              SuccessExecutionResult());
    EXPECT_EQ(
        consume_budget_transaction_context.response->last_execution_timestamp,
        1234567890123456789);
    is_called = true;
  };

  privacy_budget_service_client.OnInitiateConsumeBudgetTransactionCallback(
      consume_budget_transaction_context, http_context);

  EXPECT_EQ(is_called, true);
}

void PBSClientTest::ExecuteTransactionPhaseHelper(
    TransactionExecutionPhase phase, string phase_str) {
  PrivacyBudgetServiceClient privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);
  mock_async_executor_->schedule_for_mock =
      [](const core::AsyncOperation& work, core::Timestamp,
         std::function<bool()>&) { return SuccessExecutionResult(); };
  privacy_budget_service_client.Init();
  privacy_budget_service_client.Run();

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();
  transaction_phase_context.request->transaction_origin =
      make_shared<string>("This is transaction origin");
  transaction_phase_context.request->transaction_id = Uuid::GenerateUuid();
  transaction_phase_context.request->transaction_secret =
      make_shared<string>("This is secret");
  transaction_phase_context.request->transaction_execution_phase = phase;
  transaction_phase_context.request->last_execution_timestamp = 1234567890;

  bool is_called = false;
  mock_http_client_->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        EXPECT_EQ(
            *http_context.request->path,
            string("http://www.pbs_endpoint.com/v1/transactions:") + phase_str);
        EXPECT_EQ(http_context.request->method, HttpMethod::POST);
        EXPECT_NE(http_context.request->headers->find(string(core::kAuthHeader))
                      ->second.size(),
                  0);
        EXPECT_EQ(http_context.request->headers
                      ->find(string(core::kClaimedIdentityHeader))
                      ->second,
                  reporting_origin_);
        EXPECT_EQ(
            http_context.request->headers->find(string(kTransactionIdHeader))
                ->second,
            ToString(transaction_phase_context.request->transaction_id));
        EXPECT_EQ(http_context.request->headers
                      ->find(string(kTransactionSecretHeader))
                      ->second,
                  "This is secret");
        EXPECT_EQ(http_context.request->headers
                      ->find(string(kTransactionOriginHeader))
                      ->second,
                  "This is transaction origin");
        EXPECT_EQ(http_context.request->headers
                      ->find(string(kTransactionLastExecutionTimestampHeader))
                      ->second,
                  "1234567890");

        is_called = true;
        return SuccessExecutionResult();
      };

  EXPECT_EQ(privacy_budget_service_client.ExecuteTransactionPhase(
                transaction_phase_context),
            SuccessExecutionResult());

  EXPECT_EQ(is_called, true);
}

TEST_F(PBSClientTest, ExecuteTransactionPhase) {
  ExecuteTransactionPhaseHelper(TransactionExecutionPhase::Prepare, "prepare");
  ExecuteTransactionPhaseHelper(TransactionExecutionPhase::Commit, "commit");
  ExecuteTransactionPhaseHelper(TransactionExecutionPhase::Notify, "notify");
  ExecuteTransactionPhaseHelper(TransactionExecutionPhase::Abort, "abort");
  ExecuteTransactionPhaseHelper(TransactionExecutionPhase::End, "end");
}

TEST_F(PBSClientTest, ExecuteTransactionPhaseHttpFailure) {
  PrivacyBudgetServiceClient privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = make_shared<TransactionPhaseRequest>();

  transaction_phase_context.request->transaction_id = Uuid::GenerateUuid();
  transaction_phase_context.request->transaction_secret =
      make_shared<string>("This is secret");
  transaction_phase_context.request->transaction_origin =
      make_shared<string>("This is transaction origin");
  transaction_phase_context.request->transaction_execution_phase =
      TransactionExecutionPhase::End;
  transaction_phase_context.request->last_execution_timestamp = 1234567890;

  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    bool is_called = false;
    mock_http_client_->perform_request_mock =
        [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
          is_called = true;
          return result;
        };

    EXPECT_EQ(privacy_budget_service_client.ExecuteTransactionPhase(
                  transaction_phase_context),
              result);

    EXPECT_EQ(is_called, true);
  }
}

TEST_F(PBSClientTest, OnExecuteTransactionPhaseCallbackHttpFailure) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  vector<ExecutionResult> results = {FailureExecutionResult(123),
                                     RetryExecutionResult(1234)};

  for (auto result : results) {
    bool is_called = false;
    http_context.result = result;
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
        transaction_phase_context;
    transaction_phase_context.callback =
        [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
                transaction_phase_context) {
          EXPECT_THAT(transaction_phase_context.result, ResultIs(result));
          is_called = true;
        };

    privacy_budget_service_client.OnExecuteTransactionPhaseCallback(
        transaction_phase_context, http_context);

    EXPECT_EQ(is_called, true);
  }
}

TEST_F(PBSClientTest, OnExecuteTransactionPhaseCallbackHttpNoHeader) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  http_context.response->headers = make_shared<HttpHeaders>();

  bool is_called = false;
  http_context.result = SuccessExecutionResult();
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.callback =
      [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) {
        EXPECT_EQ(transaction_phase_context.result,
                  FailureExecutionResult(
                      core::errors::SC_PBS_CLIENT_RESPONSE_HEADER_NOT_FOUND));
        is_called = true;
      };

  privacy_budget_service_client.OnExecuteTransactionPhaseCallback(
      transaction_phase_context, http_context);

  EXPECT_EQ(is_called, true);

  is_called = false;
  http_context.result = SuccessExecutionResult();
  http_context.response->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader), "dasdas"});

  transaction_phase_context.callback =
      [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) {
        EXPECT_EQ(transaction_phase_context.result,
                  FailureExecutionResult(
                      core::errors::SC_PBS_CLIENT_INVALID_RESPONSE_HEADER));
        is_called = true;
      };

  privacy_budget_service_client.OnExecuteTransactionPhaseCallback(
      transaction_phase_context, http_context);

  EXPECT_EQ(is_called, true);

  is_called = false;
  http_context.result = SuccessExecutionResult();
  http_context.response->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader),
       "123456789012345678901"});

  transaction_phase_context.callback =
      [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) {
        EXPECT_EQ(transaction_phase_context.result,
                  FailureExecutionResult(
                      core::errors::SC_PBS_CLIENT_INVALID_RESPONSE_HEADER));
        is_called = true;
      };

  privacy_budget_service_client.OnExecuteTransactionPhaseCallback(
      transaction_phase_context, http_context);

  EXPECT_EQ(is_called, true);
}

TEST_F(PBSClientTest, OnExecuteTransactionPhaseCallback) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  http_context.response->headers = make_shared<HttpHeaders>();

  bool is_called = false;

  http_context.result = SuccessExecutionResult();
  http_context.response->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader),
       "1234567890123456789"});

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.callback =
      [&](AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
              transaction_phase_context) {
        EXPECT_SUCCESS(transaction_phase_context.result);
        EXPECT_EQ(transaction_phase_context.response->last_execution_timestamp,
                  1234567890123456789);
        is_called = true;
      };

  privacy_budget_service_client.OnExecuteTransactionPhaseCallback(
      transaction_phase_context, http_context);

  EXPECT_EQ(is_called, true);
}

TEST_F(PBSClientTest,
       UnableToObtainTokenWillFailInitiateConsumeBudgetTransaction) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  ON_CALL(*mock_auth_token_provider_cache_, GetToken)
      .WillByDefault(Return(FailureExecutionResult(123)));

  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context.request =
      GetSampleConsumeBudgetTransactionRequest();
  EXPECT_THAT(privacy_budget_service_client.Init(),
              ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(privacy_budget_service_client.Run(),
              ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(privacy_budget_service_client.InitiateConsumeBudgetTransaction(
                  consume_budget_transaction_context),
              ResultIs(FailureExecutionResult(123)));

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.request =
      GetSampleGetTransactionStatusRequest();
  EXPECT_THAT(privacy_budget_service_client.GetTransactionStatus(
                  get_transaction_status_context),
              ResultIs(FailureExecutionResult(123)));

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context;
  transaction_phase_context.request = GetSampleTransactionPhaseRequest();
  EXPECT_THAT(privacy_budget_service_client.ExecuteTransactionPhase(
                  transaction_phase_context),
              ResultIs(FailureExecutionResult(123)));
}

TEST_F(PBSClientTest, GetTransactionStatus) {
  PrivacyBudgetServiceClient privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);
  mock_async_executor_->schedule_for_mock =
      [](const core::AsyncOperation& work, core::Timestamp,
         std::function<bool()>&) { return SuccessExecutionResult(); };
  privacy_budget_service_client.Init();
  privacy_budget_service_client.Run();

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context;
  get_transaction_status_context.request =
      make_shared<GetTransactionStatusRequest>();

  get_transaction_status_context.request->transaction_id = Uuid::GenerateUuid();
  get_transaction_status_context.request->transaction_secret =
      make_shared<string>("This is secret");
  get_transaction_status_context.request->transaction_origin =
      make_shared<string>("This is transaction origin");

  bool is_called = false;
  mock_http_client_->perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        EXPECT_EQ(http_context.request->method, HttpMethod::GET);
        EXPECT_EQ(*http_context.request->path,
                  "http://www.pbs_endpoint.com/v1/transactions:status");
        auto transaction_id =
            http_context.request->headers->find(kTransactionIdHeader);
        EXPECT_EQ(transaction_id->second,
                  core::common::ToString(
                      get_transaction_status_context.request->transaction_id));
        EXPECT_NE(http_context.request->headers->find(string(core::kAuthHeader))
                      ->second.size(),
                  0);
        EXPECT_EQ(http_context.request->headers
                      ->find(string(core::kClaimedIdentityHeader))
                      ->second,
                  reporting_origin_);
        auto transaction_secret =
            http_context.request->headers->find(kTransactionSecretHeader);
        EXPECT_EQ(transaction_secret->second, "This is secret");
        auto transaction_origin =
            http_context.request->headers->find(kTransactionOriginHeader);
        EXPECT_EQ(transaction_origin->second, "This is transaction origin");
        is_called = true;
        return SuccessExecutionResult();
      };
  EXPECT_EQ(privacy_budget_service_client.GetTransactionStatus(
                get_transaction_status_context),
            SuccessExecutionResult());
  EXPECT_EQ(is_called, true);
}

TEST_F(PBSClientTest, OnGetTransactionStatusCallback) {
  MockPrivacyBudgetServiceClientWithOverrides privacy_budget_service_client(
      reporting_origin_, pbs_endpoint_, http_client_,
      auth_token_provider_cache_);

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_context;
  get_transaction_context.request = make_shared<GetTransactionStatusRequest>();
  get_transaction_context.request->transaction_id = Uuid::GenerateUuid();
  get_transaction_context.request->transaction_secret =
      make_shared<string>("transaction_secret");
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.response = make_shared<HttpResponse>();
  auto is_called = false;
  get_transaction_context.callback =
      [&](AsyncContext<GetTransactionStatusRequest,
                       GetTransactionStatusResponse>& get_transaction_context) {
        EXPECT_THAT(get_transaction_context.result,
                    ResultIs(FailureExecutionResult(1234)));
        is_called = true;
      };
  http_context.result = FailureExecutionResult(1234);
  privacy_budget_service_client.OnGetTransactionStatusCallback(
      get_transaction_context, http_context);
  EXPECT_TRUE(is_called);
  is_called = false;
  get_transaction_context.callback =
      [&](AsyncContext<GetTransactionStatusRequest,
                       GetTransactionStatusResponse>& get_transaction_context) {
        EXPECT_THAT(get_transaction_context.result,
                    ResultIs(RetryExecutionResult(1234)));
        is_called = true;
      };
  http_context.result = RetryExecutionResult(1234);
  privacy_budget_service_client.OnGetTransactionStatusCallback(
      get_transaction_context, http_context);
  EXPECT_TRUE(is_called);
  is_called = false;
  get_transaction_context.callback =
      [&](AsyncContext<GetTransactionStatusRequest,
                       GetTransactionStatusResponse>& get_transaction_context) {
        EXPECT_EQ(
            get_transaction_context.result,
            FailureExecutionResult(
                core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY));
        is_called = true;
      };
  http_context.result = SuccessExecutionResult();
  http_context.response = make_shared<HttpResponse>();
  http_context.response->body.capacity = 1;
  http_context.response->body.bytes = make_shared<vector<Byte>>();
  privacy_budget_service_client.OnGetTransactionStatusCallback(
      get_transaction_context, http_context);
  EXPECT_TRUE(is_called);
  is_called = false;
  get_transaction_context.callback =
      [&](AsyncContext<GetTransactionStatusRequest,
                       GetTransactionStatusResponse>& get_transaction_context) {
        EXPECT_SUCCESS(get_transaction_context.result);
        EXPECT_EQ(get_transaction_context.response->is_expired, false);
        EXPECT_EQ(get_transaction_context.response->has_failure, true);
        EXPECT_EQ(get_transaction_context.response->last_execution_timestamp,
                  1234512313);
        EXPECT_EQ(get_transaction_context.response->transaction_execution_phase,
                  TransactionExecutionPhase::Notify);
        is_called = true;
      };
  string get_transaction_status(
      "{\"has_failures\":true,\"is_expired\":false,\"last_execution_"
      "timestamp\":1234512313,\"transaction_execution_phase\":\"NOTIFY\"}");
  http_context.result = SuccessExecutionResult();
  http_context.response = make_shared<HttpResponse>();
  http_context.response->body.capacity = get_transaction_status.length();
  http_context.response->body.length = get_transaction_status.length();
  http_context.response->body.bytes = make_shared<vector<Byte>>(
      get_transaction_status.begin(), get_transaction_status.end());
  privacy_budget_service_client.OnGetTransactionStatusCallback(
      get_transaction_context, http_context);
  EXPECT_TRUE(is_called);
}

}  // namespace google::scp::pbs::test
