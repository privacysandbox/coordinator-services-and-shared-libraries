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

#include "pbs/partition_request_router/src/transaction_request_router_for_partition.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "core/interface/partition_namespace_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "pbs/partition/mock/pbs_partition_mock.h"
#include "pbs/partition_manager/mock/pbs_partition_manager_mock.h"
#include "pbs/partition_namespace/mock/partition_namespace_mock.h"
#include "pbs/partition_request_router/src/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::CheckpointLog;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::PartitionId;
using google::scp::core::ResourceId;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionManagerInterface;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionRequestRouterInterface;
using google::scp::core::TransactionResponse;
using google::scp::core::common::Uuid;
using google::scp::core::test::ResultIs;
using google::scp::pbs::partition::mock::MockPBSPartition;
using google::scp::pbs::partition_manager::mock::MockPBSPartitionManager;
using google::scp::pbs::partition_namespace::mock::MockPartitionNamespace;
using std::shared_ptr;
using ::testing::_;
using ::testing::An;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::ReturnRef;

namespace google::scp::pbs::test {

class TransactionRequestRouterForPartitionTest : public testing::Test {
 protected:
  TransactionRequestRouterForPartitionTest() {
    partition_mock_ = std::make_shared<MockPBSPartition>();
    partition_manager_mock_ = std::make_shared<MockPBSPartitionManager>();
    partition_namespace_mock_ = std::make_shared<MockPartitionNamespace>();
    transaction_request_router_ =
        std::make_unique<TransactionRequestRouterForPartition>(
            partition_namespace_mock_, partition_manager_mock_);
  }

  shared_ptr<MockPBSPartitionManager> partition_manager_mock_;
  shared_ptr<MockPartitionNamespace> partition_namespace_mock_;
  shared_ptr<MockPBSPartition> partition_mock_;
  std::unique_ptr<TransactionRequestRouterInterface>
      transaction_request_router_;

  const PartitionId partition_id_1_ = {11, 22};
  const PartitionId partition_id_2_ = {11, 23};
  const PartitionId partition_id_3_ = {11, 24};
  const std::vector<PartitionId> one_partition_ = {partition_id_1_};
  const std::vector<PartitionId> two_partitions_ = {partition_id_1_,
                                                    partition_id_2_};
  const std::vector<PartitionId> three_partitions_ = {
      partition_id_1_, partition_id_2_, partition_id_3_};
};

TEST_F(TransactionRequestRouterForPartitionTest, ExecuteTransactionRequest) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(partition_mock_));
  EXPECT_CALL(*partition_namespace_mock_, MapResourceToPartition("origin"))
      .WillOnce(Return(partition_id_1_));
  EXPECT_CALL(*partition_mock_,
              ExecuteRequest(
                  An<AsyncContext<TransactionRequest, TransactionResponse>&>()))
      .WillOnce(Return(SuccessExecutionResult()));

  AsyncContext<TransactionRequest, TransactionResponse> context(
      std::make_shared<TransactionRequest>(), [](auto&) {});
  context.request->transaction_origin = std::make_shared<std::string>("origin");
  EXPECT_SUCCESS(transaction_request_router_->Execute(context));
}

TEST_F(TransactionRequestRouterForPartitionTest,
       ExecuteTransactionRequestPartitionUnavailable) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(FailureExecutionResult(1234)));
  EXPECT_CALL(*partition_namespace_mock_, MapResourceToPartition("origin"))
      .WillOnce(Return(partition_id_1_));

  AsyncContext<TransactionRequest, TransactionResponse> context(
      std::make_shared<TransactionRequest>(), [](auto&) {});
  context.request->transaction_origin = std::make_shared<std::string>("origin");
  EXPECT_THAT(
      transaction_request_router_->Execute(context),
      ResultIs(FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE)));
}

TEST_F(TransactionRequestRouterForPartitionTest,
       ExecuteTransactionPhaseRequest) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(partition_mock_));
  EXPECT_CALL(*partition_namespace_mock_, MapResourceToPartition("origin"))
      .WillOnce(Return(partition_id_1_));
  EXPECT_CALL(*partition_mock_,
              ExecuteRequest(An<AsyncContext<TransactionPhaseRequest,
                                             TransactionPhaseResponse>&>()))
      .WillOnce(Return(SuccessExecutionResult()));

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse> context(
      std::make_shared<TransactionPhaseRequest>(), [](auto&) {});
  context.request->transaction_origin = std::make_shared<std::string>("origin");
  EXPECT_SUCCESS(transaction_request_router_->Execute(context));
}

TEST_F(TransactionRequestRouterForPartitionTest,
       ExecuteTransactionPhaseRequestPartitionUnavailable) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(FailureExecutionResult(1234)));
  EXPECT_CALL(*partition_namespace_mock_, MapResourceToPartition("origin"))
      .WillOnce(Return(partition_id_1_));

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse> context(
      std::make_shared<TransactionPhaseRequest>(), [](auto&) {});
  context.request->transaction_origin = std::make_shared<std::string>("origin");
  EXPECT_THAT(
      transaction_request_router_->Execute(context),
      ResultIs(FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE)));
}

TEST_F(TransactionRequestRouterForPartitionTest, GetTransactionStatusRequest) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(partition_mock_));
  EXPECT_CALL(*partition_namespace_mock_, MapResourceToPartition("origin"))
      .WillOnce(Return(partition_id_1_));
  EXPECT_CALL(*partition_mock_, GetTransactionStatus)
      .WillOnce(Return(SuccessExecutionResult()));

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      context(std::make_shared<GetTransactionStatusRequest>(), [](auto&) {});
  context.request->transaction_origin = std::make_shared<std::string>("origin");
  EXPECT_SUCCESS(transaction_request_router_->Execute(context));
}

TEST_F(TransactionRequestRouterForPartitionTest,
       GetTransactionStatusRequestPartitionUnavailable) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(FailureExecutionResult(1234)));
  EXPECT_CALL(*partition_namespace_mock_, MapResourceToPartition("origin"))
      .WillOnce(Return(partition_id_1_));

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      context(std::make_shared<GetTransactionStatusRequest>(), [](auto&) {});
  context.request->transaction_origin = std::make_shared<std::string>("origin");
  EXPECT_THAT(
      transaction_request_router_->Execute(context),
      ResultIs(FailureExecutionResult(
          core::errors::
              SC_PBS_TRANSACTION_REQUEST_ROUTER_PARTITION_UNAVAILABLE)));
}

TEST_F(TransactionRequestRouterForPartitionTest,
       GetTransactionManagerStatusRequest) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(partition_mock_));
  EXPECT_CALL(*partition_namespace_mock_, GetPartitions)
      .WillOnce(ReturnRef(one_partition_));
  EXPECT_CALL(*partition_mock_, GetTransactionManagerStatus)
      .WillOnce([](const GetTransactionManagerStatusRequest&,
                   GetTransactionManagerStatusResponse& response) {
        response.pending_transactions_count = 1230;
        return SuccessExecutionResult();
      });
  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;
  EXPECT_SUCCESS(transaction_request_router_->Execute(request, response));
  EXPECT_EQ(response.pending_transactions_count, 1230);
}

TEST_F(TransactionRequestRouterForPartitionTest,
       GetTransactionManagerStatusRequestWhenSomePartitionsNotAvailable) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition)
      .Times(2)
      .WillRepeatedly(Return(FailureExecutionResult(
          core::errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(partition_mock_));
  EXPECT_CALL(*partition_namespace_mock_, GetPartitions)
      .WillOnce(ReturnRef(three_partitions_));
  EXPECT_CALL(*partition_mock_, GetTransactionManagerStatus)
      .WillOnce([](const GetTransactionManagerStatusRequest&,
                   GetTransactionManagerStatusResponse& response) {
        response.pending_transactions_count = 1230;
        return SuccessExecutionResult();
      });

  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;
  EXPECT_SUCCESS(transaction_request_router_->Execute(request, response));
  EXPECT_EQ(response.pending_transactions_count, 1230);
}

TEST_F(TransactionRequestRouterForPartitionTest,
       GetTransactionManagerStatusRequestWhenErrorInObtainingPartition) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition)
      .WillOnce(Return(FailureExecutionResult(1234)));
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition(partition_id_1_))
      .WillOnce(Return(partition_mock_));
  EXPECT_CALL(*partition_namespace_mock_, GetPartitions)
      .WillOnce(ReturnRef(two_partitions_));
  EXPECT_CALL(*partition_mock_, GetTransactionManagerStatus)
      .WillOnce([](const GetTransactionManagerStatusRequest&,
                   GetTransactionManagerStatusResponse& response) {
        response.pending_transactions_count = 1230;
        return SuccessExecutionResult();
      });

  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;
  EXPECT_THAT(transaction_request_router_->Execute(request, response),
              ResultIs(FailureExecutionResult(1234)));
  EXPECT_NE(response.pending_transactions_count, 1230);
}

TEST_F(TransactionRequestRouterForPartitionTest,
       GetTransactionManagerStatusRequestErrorInObtainingTMStatus) {
  EXPECT_CALL(*partition_manager_mock_, GetPBSPartition)
      .WillRepeatedly(Return(partition_mock_));
  EXPECT_CALL(*partition_namespace_mock_, GetPartitions)
      .WillOnce(ReturnRef(two_partitions_));
  EXPECT_CALL(*partition_mock_, GetTransactionManagerStatus)
      .WillOnce(Return(FailureExecutionResult(1234)));

  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;
  EXPECT_THAT(transaction_request_router_->Execute(request, response),
              ResultIs(FailureExecutionResult(1234)));
  EXPECT_NE(response.pending_transactions_count, 1230);
}

}  // namespace google::scp::pbs::test
