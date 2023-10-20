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

#include <atomic>
#include <cassert>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/PutObjectRequest.h>

#include "core/async_executor/src/async_executor.h"
#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/blob_storage_provider/src/aws/aws_s3.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/interface/remote_transaction_manager_interface.h"
#include "core/journal_service/src/journal_service.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider.h"
#include "core/transaction_manager/src/transaction_manager.h"
#include "pbs/budget_key_provider/src/budget_key_provider.h"
#include "pbs/transactions/src/transaction_command_serializer.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"

using Aws::InitAPI;
using Aws::MakeShared;
using Aws::SDKOptions;

using Aws::ShutdownAPI;
using Aws::String;
using Aws::Client::ClientConfiguration;
using Aws::S3::S3Client;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::BlobStorageClientInterface;
using google::scp::core::BlobStorageProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::JournalRecoverRequest;
using google::scp::core::JournalRecoverResponse;
using google::scp::core::JournalService;
using google::scp::core::JournalServiceInterface;
using google::scp::core::LoggerInterface;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::RemoteTransactionManagerInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TransactionCommandSerializerInterface;
using google::scp::core::TransactionManager;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::blob_storage_provider::AwsS3Client;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::ConsoleLogProvider;
using google::scp::core::logger::Logger;

using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProvider;
using google::scp::cpio::MockMetricClient;
using google::scp::pbs::BudgetKeyProvider;
using google::scp::pbs::TransactionCommandSerializer;
using std::atomic;
using std::cout;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_unique;
using std::move;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::chrono::seconds;
using std::this_thread::sleep_for;

class MockBlobStorageProvider : public BlobStorageProviderInterface {
 public:
  MockBlobStorageProvider(const string& region_name,
                          shared_ptr<AsyncExecutorInterface> async_executor_io,
                          shared_ptr<AsyncExecutorInterface> async_executor)
      : region_name_(region_name),
        async_executor_io_(async_executor_io),
        async_executor_(async_executor) {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult CreateBlobStorageClient(
      shared_ptr<BlobStorageClientInterface>& blob_storage_client) noexcept
      override {
    Aws::Client::ClientConfiguration client_config;
    client_config.maxConnections = 100;
    client_config.executor = make_shared<AwsAsyncExecutor>(async_executor_io_);
    client_config.region = String(region_name_.c_str());

    auto s3_client = make_shared<S3Client>(move(client_config));
    blob_storage_client = make_shared<AwsS3Client>(s3_client, async_executor_);
    return SuccessExecutionResult();
  }

 private:
  string region_name_;
  shared_ptr<AsyncExecutorInterface> async_executor_io_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
};

/**
 * @brief This test needs to be run manually with AWS credentials configured for
 * the `region_name` specified below. `bucket_name` refers to the S3 bucket on
 * the region where journals are stored. `partition_name` refers to the PBS
 * partition for which journals needs to be read.
 */
int main(int argc, char* argv[]) {
  SDKOptions sdk_options;
  sdk_options.httpOptions.installSigPipeHandler = true;
  InitAPI(sdk_options);

  unique_ptr<LoggerInterface> logger_ptr =
      make_unique<Logger>(make_unique<ConsoleLogProvider>());
  assert(logger_ptr->Init().Successful());
  assert(logger_ptr->Run().Successful());
  GlobalLogger::SetGlobalLogger(std::move(logger_ptr));

  if (argc < 2) {
    std::cerr << "Must provide two parameters, 1. Journals S3 bucket name and "
                 "2. Configured AWS "
                 "Region\n";
    return -1;
  }

  string region_name = argv[2];
  auto bucket_name = make_shared<string>(argv[1]);
  auto partition_name =
      make_shared<string>("00000000-0000-0000-0000-000000000000");

  auto async_executor_io = make_shared<AsyncExecutor>(200, 100000);
  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<AsyncExecutor>(40, 100000, true /* drop_tasks_on_stop */);
  shared_ptr<BlobStorageProviderInterface> mock_blob_storage_provider =
      make_shared<MockBlobStorageProvider>(region_name, async_executor_io,
                                           async_executor);

  auto mock_metric_client = make_shared<MockMetricClient>();

  assert(async_executor_io->Init().Successful());
  assert(async_executor_io->Run().Successful());

  assert(mock_blob_storage_provider->Init().Successful());
  assert(mock_blob_storage_provider->Run().Successful());

  assert(async_executor->Init().Successful());
  assert(async_executor->Run().Successful());

  assert(mock_metric_client->Init().Successful());
  assert(mock_metric_client->Run().Successful());

  auto mock_config_provider = make_shared<MockConfigProvider>();
  shared_ptr<JournalServiceInterface> journal_service =
      make_shared<JournalService>(bucket_name, partition_name, async_executor,
                                  mock_blob_storage_provider,
                                  mock_metric_client, mock_config_provider);

  assert(journal_service->Init().Successful());

  shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider =
      make_shared<MockNoSQLDatabaseProvider>();
  assert(nosql_database_provider->Init().Successful());
  assert(nosql_database_provider->Run().Successful());

  auto budget_key_provider = make_shared<BudgetKeyProvider>(
      async_executor, journal_service, nosql_database_provider,
      mock_metric_client, mock_config_provider);
  assert(budget_key_provider->Init().Successful());

  shared_ptr<TransactionCommandSerializerInterface>
      transaction_command_serializer =
          make_shared<TransactionCommandSerializer>(async_executor,
                                                    budget_key_provider);
  shared_ptr<RemoteTransactionManagerInterface> remote_transaction_manager =
      nullptr;
  auto transaction_manager = make_shared<TransactionManager>(
      async_executor, transaction_command_serializer, journal_service,
      remote_transaction_manager, 10000000, mock_metric_client,
      mock_config_provider);
  assert(transaction_manager->Init().Successful());

  atomic<bool> recovery_done(false);
  AsyncContext<JournalRecoverRequest, JournalRecoverResponse> recovery_context;
  recovery_context.request = make_shared<JournalRecoverRequest>();
  recovery_context.callback =
      [&](AsyncContext<JournalRecoverRequest, JournalRecoverResponse>&
              recovery_context) {
        assert(recovery_context.result.Successful());
        cout << "Last recovered journal ID "
             << recovery_context.response->last_processed_journal_id
             << std::endl;
        recovery_done = true;
      };
  assert(journal_service->Recover(recovery_context).Successful());

  while (!recovery_done) {
    sleep_for(seconds(1));
  }

  cout << "Recovery Done\n";
  assert(journal_service->Run().Successful());
  assert(budget_key_provider->Run().Successful());
  assert(transaction_manager->Run().Successful());

  cout << "Stopping...\n";
  assert(journal_service->Stop().Successful());
  assert(budget_key_provider->Stop().Successful());
  assert(transaction_manager->Stop().Successful());
  assert(mock_metric_client->Stop().Successful());
  assert(mock_blob_storage_provider->Stop().Successful());
  assert(async_executor_io->Stop().Successful());
  assert(async_executor->Stop().Successful());

  return 0;
}
