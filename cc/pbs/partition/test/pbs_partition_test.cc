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

#include "pbs/partition/src/pbs_partition.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_context.h"
#include "core/interface/partition_interface.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "core/transaction_manager/src/error_codes.h"
#include "pbs/partition/src/error_codes.h"
#include "pbs/transactions/src/consume_budget_command.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionCommand;
using google::scp::core::TransactionCommandCallback;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::core::test::ResultIs;
using google::scp::core::test::TestLoggingUtils;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::ConsumeBudgetCommandRequestInfo;

static constexpr char kPartitionsBucketName[] = "partitions";

namespace google::scp::pbs::test {
class PBSPartitionTest : public testing::Test {
 protected:
  std::shared_ptr<AsyncExecutor> async_executor_;
  std::shared_ptr<MockBlobStorageProvider> blob_store_provider_;
  std::shared_ptr<MockMetricClient> metric_client_provider_;
  std::shared_ptr<MockConfigProvider> config_provider_;
  std::shared_ptr<MockNoSQLDatabaseProvider> nosql_database_provider_;

  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
  GetSampleTransactionPhaseRequestContext(const TransactionRequest& request,
                                          const TransactionExecutionPhase phase,
                                          size_t last_execution_timestamp) {
    auto context =
        AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>(
            std::make_shared<TransactionPhaseRequest>(), [](auto&) {});
    context.request->transaction_id = request.transaction_id;
    context.request->transaction_origin = request.transaction_origin;
    context.request->transaction_secret = request.transaction_secret;
    context.request->transaction_execution_phase = phase;
    context.request->last_execution_timestamp = last_execution_timestamp;
    return context;
  }

  AsyncContext<TransactionRequest, TransactionResponse>
  GetSampleTransactionRequestContext() {
    static std::atomic<size_t> budget_key_unique_counter = 0;
    auto budget_key = std::make_shared<std::string>(
        "budget_key" + std::to_string(budget_key_unique_counter++));
    TimeBucket time_bucket = 100;
    TokenCount token_count = 1;
    auto request = std::make_shared<TransactionRequest>();
    request->is_coordinated_remotely = true;
    request->transaction_id = Uuid::GenerateUuid();
    request->transaction_origin = std::make_shared<std::string>("origin");
    request->transaction_secret = std::make_shared<std::string>("secret");
    auto context = AsyncContext<TransactionRequest, TransactionResponse>(
        request, [](auto&) {});
    context.request->commands.push_back(std::make_shared<ConsumeBudgetCommand>(
        request->transaction_id, budget_key,
        ConsumeBudgetCommandRequestInfo(time_bucket, token_count)));
    return context;
  }

  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
  GetSampleGetTransactionStatusRequestContext() {
    return AsyncContext<GetTransactionStatusRequest,
                        GetTransactionStatusResponse>(
        std::make_shared<GetTransactionStatusRequest>(), [](auto&) {});
  }

  void SetUp() override {
    async_executor_ = std::make_shared<AsyncExecutor>(
        4 /* threads */, 10000 /* queue size */, true /* drop_tasks_on_stop */);
    blob_store_provider_ = std::make_shared<MockBlobStorageProvider>();
    metric_client_provider_ = std::make_shared<MockMetricClient>();
    config_provider_ = std::make_shared<MockConfigProvider>();
    nosql_database_provider_ = std::make_shared<MockNoSQLDatabaseProvider>();

    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(blob_store_provider_->Init());
    EXPECT_SUCCESS(metric_client_provider_->Init());
    EXPECT_SUCCESS(config_provider_->Init());
    EXPECT_SUCCESS(nosql_database_provider_->Init());

    EXPECT_SUCCESS(async_executor_->Run());
    EXPECT_SUCCESS(blob_store_provider_->Run());
    EXPECT_SUCCESS(metric_client_provider_->Run());
    EXPECT_SUCCESS(config_provider_->Run());
    EXPECT_SUCCESS(nosql_database_provider_->Run());

    TestLoggingUtils::EnableLogOutputToConsole();

    dependencies_.async_executor = async_executor_;
    dependencies_.blob_store_provider = blob_store_provider_;
    dependencies_.blob_store_provider_for_checkpoints = blob_store_provider_;
    dependencies_.config_provider = config_provider_;
    dependencies_.metric_client = metric_client_provider_;
    dependencies_.nosql_database_provider_for_background_operations =
        nosql_database_provider_;
    dependencies_.nosql_database_provider_for_live_traffic =
        nosql_database_provider_;
    dependencies_.remote_transaction_manager = nullptr;

    partition_ = std::make_shared<PBSPartition>(partition_id_, dependencies_,
                                                journal_bucket_name_,
                                                transaction_manager_capacity_);

    dummy_transaction_request_ = GetSampleTransactionRequestContext();
    dummy_transaction_phase_request_ = GetSampleTransactionPhaseRequestContext(
        *dummy_transaction_request_.request, TransactionExecutionPhase::Begin,
        0 /* last execution timestamp */);
    dummy_transaction_status_request_ =
        GetSampleGetTransactionStatusRequestContext();
  }

  void TearDown() override {
    EXPECT_SUCCESS(async_executor_->Stop());
    EXPECT_SUCCESS(blob_store_provider_->Stop());
    EXPECT_SUCCESS(metric_client_provider_->Stop());
    EXPECT_SUCCESS(config_provider_->Stop());
    EXPECT_SUCCESS(nosql_database_provider_->Stop());
  }

  static void SetUpTestSuite() {
    std::filesystem::path dir = kPartitionsBucketName;
    std::filesystem::create_directory(dir);
  }

  void ExecuteAllRequestTypes(ExecutionResult expected_result) {
    EXPECT_THAT(partition_->ExecuteRequest(dummy_transaction_phase_request_),
                ResultIs(expected_result));
    EXPECT_THAT(partition_->ExecuteRequest(dummy_transaction_request_),
                ResultIs(expected_result));
    EXPECT_THAT(
        partition_->GetTransactionStatus(dummy_transaction_status_request_),
        ResultIs(expected_result));
    EXPECT_THAT(partition_->GetTransactionManagerStatus(
                    dummy_transaction_manager_status_request_,
                    dummy_transaction_manager_status_response_),
                ResultIs(expected_result));
  }

  std::shared_ptr<PBSPartition> partition_;
  core::PartitionId partition_id_ = Uuid::GenerateUuid();
  size_t transaction_manager_capacity_ = 100000;
  std::shared_ptr<std::string> journal_bucket_name_ =
      std::make_shared<std::string>(kPartitionsBucketName);
  PBSPartition::Dependencies dependencies_;

  // Request objects
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      dummy_transaction_phase_request_;
  AsyncContext<TransactionRequest, TransactionResponse>
      dummy_transaction_request_;
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      dummy_transaction_status_request_;
  GetTransactionManagerStatusRequest dummy_transaction_manager_status_request_;
  GetTransactionManagerStatusResponse
      dummy_transaction_manager_status_response_;
};

TEST_F(PBSPartitionTest, PartitionDoesNotAcceptRequestsToBeginWith) {
  auto expected_result =
      RetryExecutionResult(core::errors::SC_PBS_PARTITION_NOT_LOADED);
  ExecuteAllRequestTypes(expected_result);
}

TEST_F(PBSPartitionTest, PartitionDoesNotAcceptRequestsBeforeLoad) {
  EXPECT_SUCCESS(partition_->Init());

  auto expected_result =
      RetryExecutionResult(core::errors::SC_PBS_PARTITION_NOT_LOADED);
  ExecuteAllRequestTypes(expected_result);
}

TEST_F(PBSPartitionTest, PartitionCannotAcceptRequestsAfterUnload) {
  EXPECT_SUCCESS(partition_->Init());
  EXPECT_SUCCESS(partition_->Load());
  EXPECT_SUCCESS(partition_->Unload());

  auto expected_result =
      RetryExecutionResult(core::errors::SC_PBS_PARTITION_NOT_LOADED);
  ExecuteAllRequestTypes(expected_result);
}

TEST_F(PBSPartitionTest, PartitionCannotLoadUntilInitialized) {
  auto expected_result = ResultIs(
      FailureExecutionResult(core::errors::SC_PBS_PARTITION_CANNOT_LOAD));
  EXPECT_THAT(partition_->Load(), expected_result);

  EXPECT_SUCCESS(partition_->Init());
  EXPECT_SUCCESS(partition_->Load());
  EXPECT_SUCCESS(partition_->Unload());
}

TEST_F(PBSPartitionTest, PartitionCannotUnloadUntilLoaded) {
  auto expected_result = ResultIs(
      FailureExecutionResult(core::errors::SC_PBS_PARTITION_CANNOT_UNLOAD));
  EXPECT_THAT(partition_->Unload(), expected_result);

  EXPECT_SUCCESS(partition_->Init());
  EXPECT_THAT(partition_->Unload(), expected_result);

  EXPECT_SUCCESS(partition_->Load());
  EXPECT_SUCCESS(partition_->Unload());
}

TEST_F(PBSPartitionTest, PartitionAcceptsTransactionPhaseRequestAfterLoad) {
  EXPECT_SUCCESS(partition_->Init());
  EXPECT_SUCCESS(partition_->Load());

  EXPECT_SUCCESS(partition_->ExecuteRequest(dummy_transaction_phase_request_));

  EXPECT_SUCCESS(partition_->Unload());
}

TEST_F(PBSPartitionTest, PartitionAcceptsGetTransactionStatusAfterLoad) {
  EXPECT_SUCCESS(partition_->Init());
  EXPECT_SUCCESS(partition_->Load());

  EXPECT_THAT(
      partition_->GetTransactionStatus(dummy_transaction_status_request_),
      ResultIs(FailureExecutionResult(
          core::errors::SC_TRANSACTION_MANAGER_TRANSACTION_NOT_FOUND)));

  EXPECT_SUCCESS(partition_->Unload());
}

TEST_F(PBSPartitionTest, PartitionAcceptsGetTransactionManagerStatusAfterLoad) {
  EXPECT_SUCCESS(partition_->Init());
  EXPECT_SUCCESS(partition_->Load());

  EXPECT_SUCCESS(partition_->GetTransactionManagerStatus(
      dummy_transaction_manager_status_request_,
      dummy_transaction_manager_status_response_));

  EXPECT_SUCCESS(partition_->Unload());
}

TEST_F(PBSPartitionTest, PartitionAcceptsTransactionRequestAfterLoad) {
  EXPECT_SUCCESS(partition_->Init());
  EXPECT_SUCCESS(partition_->Load());

  EXPECT_SUCCESS(partition_->ExecuteRequest(dummy_transaction_request_));

  EXPECT_SUCCESS(partition_->Unload());
}

TEST_F(PBSPartitionTest, MultiPartitionLoadUnloadIsSuccessful) {
  auto partition_id1 = Uuid::GenerateUuid();
  auto partition_id2 = Uuid::GenerateUuid();
  auto partition_id3 = Uuid::GenerateUuid();

  auto partition1 = std::make_shared<PBSPartition>(
      partition_id1, dependencies_, journal_bucket_name_,
      transaction_manager_capacity_);
  auto partition2 = std::make_shared<PBSPartition>(
      partition_id2, dependencies_, journal_bucket_name_,
      transaction_manager_capacity_);
  auto partition3 = std::make_shared<PBSPartition>(
      partition_id3, dependencies_, journal_bucket_name_,
      transaction_manager_capacity_);

  EXPECT_SUCCESS(partition1->Init());
  EXPECT_SUCCESS(partition2->Init());
  EXPECT_SUCCESS(partition3->Init());

  EXPECT_SUCCESS(partition1->Load());
  EXPECT_SUCCESS(partition2->Load());
  EXPECT_SUCCESS(partition3->Load());

  for (int i = 0; i < 1000; i++) {
    auto request1 = GetSampleTransactionRequestContext();
    EXPECT_SUCCESS(partition1->ExecuteRequest(request1));
    auto request2 = GetSampleTransactionRequestContext();
    EXPECT_SUCCESS(partition2->ExecuteRequest(request2));
    auto request3 = GetSampleTransactionRequestContext();
    EXPECT_SUCCESS(partition3->ExecuteRequest(request3));
  }

  EXPECT_SUCCESS(partition1->Unload());
  EXPECT_SUCCESS(partition2->Unload());
  EXPECT_SUCCESS(partition3->Unload());
}

TEST_F(PBSPartitionTest, ConcurrentLoadUnloadTest) {
  auto partition_id = Uuid::GenerateUuid();
  auto partition = std::make_shared<PBSPartition>(
      partition_id, dependencies_, journal_bucket_name_,
      transaction_manager_capacity_);

  std::atomic<bool> should_load = false;
  std::atomic<bool> should_unload = false;
  std::atomic<bool> should_init = false;

  std::atomic<size_t> init_count = 0;
  std::atomic<size_t> load_count = 0;
  std::atomic<size_t> unload_count = 0;

  std::thread init_thread([&]() {
    while (!should_init) {}
    while (should_init) {
      partition->Init();
      init_count++;
    }
  });
  std::thread load_thread([&]() {
    while (!should_load) {}
    while (should_load) {
      partition->Load();
      load_count++;
    }
  });
  std::thread unload_thread([&]() {
    while (!should_unload) {}
    while (should_unload) {
      partition->Unload();
      unload_count++;
    }
  });

  should_init = true;
  should_load = true;
  should_unload = true;

  const size_t inits_to_be_performed = 10000;
  const size_t loads_to_be_performed = 10000;
  const size_t unloads_to_be_performed = 10000;

  WaitUntil([&]() {
    return init_count > inits_to_be_performed &&
           load_count > loads_to_be_performed &&
           unload_count > unloads_to_be_performed;
  });

  should_init = false;
  should_load = false;
  should_unload = false;

  if (init_thread.joinable()) {
    init_thread.join();
  }
  if (load_thread.joinable()) {
    load_thread.join();
  }
  if (unload_thread.joinable()) {
    unload_thread.join();
  }
}

TEST_F(PBSPartitionTest, PartitionUnloadDuringLiveTrafficIsSuccessful) {
  std::vector<std::thread> traffic_pumps;
  // Nested scope to throw away partition object after unloading.
  {
    auto partition_id = Uuid::GenerateUuid();
    auto partition = std::make_shared<PBSPartition>(
        partition_id, dependencies_, journal_bucket_name_,
        transaction_manager_capacity_);

    // Start 20 threads that pump traffic.
    std::atomic<bool> traffic_pumps_running = true;
    std::atomic<size_t> transactions_pumped = 0;
    std::atomic<size_t> transaction_phase_pumped = 0;
    std::atomic<size_t> transactions_unsuccessful = 0;
    std::atomic<size_t> transaction_phase_unsuccessful = 0;

    for (int i = 0; i < 20; i++) {
      traffic_pumps.emplace_back([&]() {
        while (traffic_pumps_running) {
          // Send a new transaction
          auto context = GetSampleTransactionRequestContext();
          context.callback = [&](auto& context) {
            if (!context.result.Successful()) {
              transactions_unsuccessful++;
              return;
            }
            // Execute a phase
            auto transaction_phase_request =
                GetSampleTransactionPhaseRequestContext(
                    *context.request, TransactionExecutionPhase::Begin,
                    context.response->last_execution_timestamp);
            transaction_phase_request.callback = [&](auto& phase_context) {
              if (!phase_context.result.Successful()) {
                transaction_phase_unsuccessful++;
                return;
              }
            };
            if (partition->ExecuteRequest(transaction_phase_request)
                    .Successful()) {
              transaction_phase_pumped.fetch_add(1, std::memory_order_relaxed);
            }
          };
          if (partition->ExecuteRequest(context).Successful()) {
            transactions_pumped.fetch_add(1, std::memory_order_relaxed);
          }
        }
      });
    }

    EXPECT_SUCCESS(partition->Init());
    EXPECT_SUCCESS(partition->Load());

    std::cout << "transactions_pumped after load: " << transactions_pumped
              << std::endl;
    std::cout << "transactions_unsuccessful after load: "
              << transactions_unsuccessful << std::endl;

    std::cout << "transaction_phase_pumped after load: "
              << transaction_phase_pumped << std::endl;
    std::cout << "transaction_phase_unsuccessful after load: "
              << transaction_phase_unsuccessful << std::endl;

    size_t total_transactions_to_pump = 20000;
    WaitUntil(
        [&]() { return transactions_pumped > total_transactions_to_pump; });
    traffic_pumps_running = false;

    // Wait enough for checkpoint service to start checkpointing..
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "transactions_pumped before unload: " << transactions_pumped
              << std::endl;
    std::cout << "transactions_unsuccessful before unload: "
              << transactions_unsuccessful << std::endl;
    std::cout << "transaction_phase_pumped before unload: "
              << transaction_phase_pumped << std::endl;
    std::cout << "transaction_phase_unsuccessful before unload: "
              << transaction_phase_unsuccessful << std::endl;
    EXPECT_SUCCESS(partition->Unload());

    std::cout << "transactions_pumped after unload: " << transactions_pumped
              << std::endl;
    std::cout << "transactions_unsuccessful after unload: "
              << transactions_unsuccessful << std::endl;
    std::cout << "transaction_phase_pumped after unload: "
              << transaction_phase_pumped << std::endl;
    std::cout << "transaction_phase_unsuccessful after unload: "
              << transaction_phase_unsuccessful << std::endl;
  }

  // Sleep for a bit and ensure no callbacks come
  std::this_thread::sleep_for(std::chrono::seconds(4));

  for (auto& pump : traffic_pumps) {
    if (pump.joinable()) {
      pump.join();
    }
  }
}
}  // namespace google::scp::pbs::test
