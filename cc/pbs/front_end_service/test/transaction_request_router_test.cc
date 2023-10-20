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

#include "pbs/front_end_service/src/transaction_request_router.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/interface/transaction_manager_interface.h"
#include "core/transaction_manager/mock/mock_transaction_manager_gmock.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::CheckpointLog;
using google::scp::core::ExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionManagerInterface;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::common::Uuid;
using google::scp::core::transaction_manager::mock::MockTransactionManagerGMock;
using testing::Return;

namespace google::scp::pbs::test {

TEST(TransactionRequestRouterTest, ExecuteTransaction) {
  auto mock_transaction_manager =
      std::make_unique<MockTransactionManagerGMock>();
  EXPECT_CALL(*mock_transaction_manager, Execute)
      .WillOnce(
          [=](AsyncContext<TransactionRequest, TransactionResponse>& context) {
            EXPECT_EQ(context.request->transaction_id.high, 1234);
            EXPECT_EQ(context.request->transaction_id.low, 1234);
            return SuccessExecutionResult();
          });

  AsyncContext<TransactionRequest, TransactionResponse> context;
  context.request = std::make_shared<TransactionRequest>();
  context.request->transaction_id = Uuid{1234, 1234};

  TransactionRequestRouter router(std::move(mock_transaction_manager));
  EXPECT_SUCCESS(router.Execute(context));
}

TEST(TransactionRequestRouterTest, ExecuteTransactionPhase) {
  auto mock_transaction_manager =
      std::make_unique<MockTransactionManagerGMock>();
  EXPECT_CALL(*mock_transaction_manager, ExecutePhase)
      .WillOnce([=](AsyncContext<TransactionPhaseRequest,
                                 TransactionPhaseResponse>& context) {
        EXPECT_EQ(context.request->transaction_id.high, 1234);
        EXPECT_EQ(context.request->transaction_id.low, 1234);
        return SuccessExecutionResult();
      });

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse> context;
  context.request = std::make_shared<TransactionPhaseRequest>();
  context.request->transaction_id = Uuid{1234, 1234};

  TransactionRequestRouter router(std::move(mock_transaction_manager));
  EXPECT_SUCCESS(router.Execute(context));
}

TEST(TransactionRequestRouterTest, ExecuteGetTransactionStatus) {
  auto mock_transaction_manager =
      std::make_unique<MockTransactionManagerGMock>();
  EXPECT_CALL(*mock_transaction_manager, GetTransactionStatus)
      .WillOnce([=](AsyncContext<GetTransactionStatusRequest,
                                 GetTransactionStatusResponse>& context) {
        EXPECT_EQ(context.request->transaction_id.high, 1234);
        EXPECT_EQ(context.request->transaction_id.low, 1234);
        return SuccessExecutionResult();
      });

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      context;
  context.request = std::make_shared<GetTransactionStatusRequest>();
  context.request->transaction_id = Uuid{1234, 1234};

  TransactionRequestRouter router(std::move(mock_transaction_manager));
  EXPECT_SUCCESS(router.Execute(context));
}

TEST(TransactionRequestRouterTest, ExecuteGetTransactionManagerStatus) {
  auto mock_transaction_manager =
      std::make_unique<MockTransactionManagerGMock>();
  EXPECT_CALL(*mock_transaction_manager, GetTransactionManagerStatus)
      .WillOnce([=](const GetTransactionManagerStatusRequest& request,
                    GetTransactionManagerStatusResponse& response) {
        response.pending_transactions_count = 123;
        return SuccessExecutionResult();
      });

  TransactionRequestRouter router(std::move(mock_transaction_manager));

  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;
  EXPECT_SUCCESS(router.Execute(request, response));
  EXPECT_EQ(response.pending_transactions_count, 123);
}

}  // namespace google::scp::pbs::test
