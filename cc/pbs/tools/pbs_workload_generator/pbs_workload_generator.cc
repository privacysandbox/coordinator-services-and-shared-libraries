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

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <aws/core/Aws.h>

#include "core/async_executor/src/async_executor.h"
#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/src/config_provider.h"
#include "core/config_provider/src/env_config_provider.h"
#include "core/credentials_provider/src/aws_credentials_provider.h"
#include "core/curl_client/src/http1_curl_client.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/authorization_service_interface.h"
#include "core/interface/type_def.h"
#include "core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/test/utils/conditional_wait.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "core/token_provider_cache/src/auto_refresh_token_provider.h"
#include "pbs/authorization_token_fetcher/src/aws/aws_authorization_token_fetcher.h"
#include "pbs/authorization_token_fetcher/src/gcp/gcp_authorization_token_fetcher.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/pbs_client/src/transactional/pbs_transactional_client.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AutoRefreshTokenProviderService;
using google::scp::core::AwsCredentialsProvider;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ConfigProvider;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::EnvConfigProvider;
using google::scp::core::ExecutionResult;
using google::scp::core::Http1CurlClient;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpClientOptions;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::kDefaultHttp2ReadTimeoutInSeconds;
using google::scp::core::kDefaultRetryStrategyDelayInMs;
using google::scp::core::kDefaultRetryStrategyMaxRetries;
using google::scp::core::LoggerInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::common::ConcurrentQueue;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::core::logger::Logger;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::core::test::WaitUntil;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::pbs::AwsAuthorizationTokenFetcher;
using google::scp::pbs::ConsumeBudgetMetadata;
using google::scp::pbs::ConsumeBudgetTransactionRequest;
using google::scp::pbs::ConsumeBudgetTransactionResponse;
using google::scp::pbs::GcpAuthorizationTokenFetcher;
using google::scp::pbs::PBSInstance;
using google::scp::pbs::PrivacyBudgetServiceTransactionalClient;
using std::atomic;
using std::condition_variable;
using std::cout;
using std::endl;
using std::getenv;
using std::ifstream;
using std::make_shared;
using std::make_unique;
using std::mt19937;
using std::mutex;
using std::random_device;
using std::runtime_error;
using std::shared_ptr;
using std::stoul;
using std::string;
using std::to_string;
using std::unique_lock;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::filesystem::path;
using std::this_thread::sleep_for;

// Do not keep retrying for a longer time as there could be a failover being
// done while workload is running. Retries are configurable via an Environment
// variable.
static constexpr size_t kHttp2RequestRetryStrategyMaxRetries = 3;

// Give up quickly if the destination node is not reachable.
static constexpr size_t kHttp2ConnectionReadTimeoutInSeconds = 5;

static constexpr char kPBSWorkloadGenerator[] = "PBSWorkloadGenerator";

namespace google::scp::pbs {

enum class CloudPlatformType { Invalid = 0, GCP = 1, AWS = 2, Local = 3 };

struct AppConfiguration {
  size_t total_transactions;
  size_t keys_per_transaction;
  path config_path;
  int64_t duration_in_seconds;
  CloudPlatformType cloud_platform_type;
};

struct SingleCoordinatorConfig {
  string reporting_origin;

  string pbs_region;
  string pbs_endpoint;
  string pbs_auth_endpoint;
};

struct MultiCoordinatorConfig {
  string reporting_origin;

  string pbs1_region;
  string pbs1_endpoint;
  string pbs1_auth_endpoint;

  string pbs2_region;
  string pbs2_endpoint;
  string pbs2_auth_endpoint;
};

unique_ptr<TokenProviderCacheInterface> GetAWSAuthTokenProviderCache(
    const string& auth_service_endpoint, const string& cloud_service_region,
    const shared_ptr<AsyncExecutorInterface>& async_executor) {
  auto credentials_provider = make_unique<AwsCredentialsProvider>();
  auto auth_token_fetcher = make_unique<AwsAuthorizationTokenFetcher>(
      auth_service_endpoint, cloud_service_region, move(credentials_provider));
  return make_unique<AutoRefreshTokenProviderService>(move(auth_token_fetcher),
                                                      async_executor);
}

unique_ptr<TokenProviderCacheInterface> GetGCPAuthTokenProviderCache(
    const shared_ptr<HttpClientInterface>& http1_client,
    const string& auth_service_endpoint,
    const shared_ptr<AsyncExecutorInterface>& async_executor) {
  auto auth_token_fetcher = make_unique<GcpAuthorizationTokenFetcher>(
      http1_client, auth_service_endpoint, async_executor);
  return make_unique<AutoRefreshTokenProviderService>(move(auth_token_fetcher),
                                                      async_executor);
}

string generate_random_string() {
  string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
  static random_device random_device_local;
  static mt19937 generator(random_device_local());
  shuffle(str.begin(), str.end(), generator);
  return str.substr(0, 10);
}

void ReadSingleCoordinatorConfig(const path& config_path,
                                 SingleCoordinatorConfig& config) {
  ConfigProvider config_provider(config_path);
  if (!config_provider.Init().Successful()) {
    throw runtime_error("Cannot initialize Config Provider");
  }

  ifstream config_file_contents(config_path.u8string());
  if (config_file_contents.is_open()) {
    cout << "Using config file:\n" << config_file_contents.rdbuf() << endl;
    config_file_contents.close();
  }

  if (!config_provider.Run().Successful()) {
    throw runtime_error("Cannot run Config Provider");
  }

  auto execution_result =
      config_provider.Get("reporting_origin", config.reporting_origin);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find reporting_origin");
  }
  execution_result = config_provider.Get("pbs_region", config.pbs_region);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs_region");
  }
  execution_result = config_provider.Get("pbs_endpoint", config.pbs_endpoint);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs_endpoint");
  }
  execution_result =
      config_provider.Get("pbs_auth_endpoint", config.pbs_auth_endpoint);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs_auth_endpoint");
  }
}

void ReadMultiCoordinatorConfig(const path& config_path,
                                MultiCoordinatorConfig& config) {
  ConfigProvider config_provider(config_path);
  if (!config_provider.Init().Successful()) {
    throw runtime_error("Cannot initialize Config Provider");
  }

  if (!config_provider.Run().Successful()) {
    throw runtime_error("Cannot run Config Provider");
  }

  auto execution_result =
      config_provider.Get("reporting_origin", config.reporting_origin);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find reporting_origin");
  }
  execution_result = config_provider.Get("pbs1_region", config.pbs1_region);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs1_region");
  }
  execution_result = config_provider.Get("pbs1_endpoint", config.pbs1_endpoint);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs1_endpoint");
  }
  execution_result =
      config_provider.Get("pbs1_auth_endpoint", config.pbs1_auth_endpoint);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs1_auth_endpoint");
  }
  execution_result = config_provider.Get("pbs2_region", config.pbs2_region);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs2_region");
  }
  execution_result = config_provider.Get("pbs2_endpoint", config.pbs2_endpoint);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs2_endpoint");
  }
  execution_result =
      config_provider.Get("pbs2_auth_endpoint", config.pbs2_auth_endpoint);
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot find pbs2_auth_endpoint");
  }
}

mutex mutex_;
atomic<bool> is_running_;
unique_ptr<std::thread> working_thread_;
unique_ptr<std::thread> key_generation_thread_;
condition_variable condition_variable_;
ConcurrentQueue<shared_ptr<string>> keys_(UINT64_MAX);
size_t total_transactions_;
// Temporary fix for the uint64_t overflow issue in the while loop later in the
// code.
atomic<int64_t> current_transaction_count_;
atomic<size_t> total_executed_;
atomic<size_t> total_failed_;

atomic<uint64_t> transactions_completed_count_;
atomic<uint64_t> previous_completed_count_;

string prefix_;  // NOLINT
atomic<uint64_t> index_;

AsyncContext<ConsumeBudgetTransactionRequest, ConsumeBudgetTransactionResponse>
CreateConsumeBudgetTransaction(const AppConfiguration& app_configuration) {
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context.request =
      make_shared<ConsumeBudgetTransactionRequest>();
  consume_budget_transaction_context.request->budget_keys =
      make_shared<vector<ConsumeBudgetMetadata>>();
  consume_budget_transaction_context.request->transaction_id =
      Uuid::GenerateUuid();

  auto val = index_.fetch_add(1);
  shared_ptr<string> key =
      make_shared<string>(prefix_ + "_" + std::to_string(val));
  consume_budget_transaction_context.request->transaction_secret = key;

  for (size_t j = 0; j < app_configuration.keys_per_transaction; ++j) {
    ConsumeBudgetMetadata metadata;

    val = index_.fetch_add(1);
    shared_ptr<string> key =
        make_shared<string>(prefix_ + "_" + std::to_string(val));

    metadata.budget_key_name = key;
    metadata.time_bucket =
        TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();
    metadata.token_count = 1;
    consume_budget_transaction_context.request->budget_keys->push_back(
        metadata);
  }

  consume_budget_transaction_context
      .callback = [&](AsyncContext<ConsumeBudgetTransactionRequest,
                                   ConsumeBudgetTransactionResponse>&
                          consume_budget_transaction_context) mutable {
    if (consume_budget_transaction_context.result == SuccessExecutionResult()) {
      return;
    }

    auto transaction_id_str =
        ToString(consume_budget_transaction_context.request->transaction_id);
    SCP_ERROR_CONTEXT(kPBSWorkloadGenerator, consume_budget_transaction_context,
                      consume_budget_transaction_context.result,
                      "The transaction failed with id: %s",
                      transaction_id_str.c_str());
  };

  return consume_budget_transaction_context;
}

void StartProducerThread(
    const AppConfiguration& app_configuration,
    const shared_ptr<PrivacyBudgetServiceTransactionalClient>& client) {
  unique_lock<mutex> thread_lock(mutex_);

  while (true) {
    condition_variable_.wait(thread_lock, [&]() {
      return !is_running_ ||
             current_transaction_count_.load() < total_transactions_;
    });

    if (!is_running_) {
      break;
    }

    thread_lock.unlock();

    // Push another one
    while (is_running_ &&
           current_transaction_count_.load() < total_transactions_) {
      auto consume_budget_transaction_context =
          CreateConsumeBudgetTransaction(app_configuration);

      auto original_callback = consume_budget_transaction_context.callback;
      consume_budget_transaction_context.callback =
          [&, original_callback](AsyncContext<ConsumeBudgetTransactionRequest,
                                              ConsumeBudgetTransactionResponse>&
                                     consume_budget_transaction_context) {
            if (consume_budget_transaction_context.result !=
                SuccessExecutionResult()) {
              total_failed_++;
            }

            total_executed_++;
            current_transaction_count_--;
            transactions_completed_count_++;
            original_callback(consume_budget_transaction_context);

            condition_variable_.notify_one();
          };

      current_transaction_count_++;
      auto execution_result =
          client->ConsumeBudget(consume_budget_transaction_context);

      if (!execution_result.Successful()) {
        SCP_ERROR(kPBSWorkloadGenerator, Uuid::GenerateUuid(), execution_result,
                  "Transaction failed to start");
        current_transaction_count_--;
        continue;
      }
    }

    thread_lock.lock();
  }
}

void RunWorkload(
    const string& reporting_origin, const AppConfiguration& app_configuration,
    const shared_ptr<PrivacyBudgetServiceTransactionalClient>& client) {
  auto execution_result = client->Init();
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot initialize the client");
  }

  execution_result = client->Run();
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot run the client");
  }

  is_running_ = true;
  total_transactions_ = app_configuration.total_transactions;
  prefix_ = generate_random_string();

  cout << "Starting the working thread." << endl;
  working_thread_ =
      make_unique<std::thread>([app_configuration, client]() mutable {
        StartProducerThread(app_configuration, client);
      });

  auto begin = steady_clock::now();
  auto end = steady_clock::now();

  while (duration_cast<seconds>(end - begin).count() <=
         app_configuration.duration_in_seconds) {
    usleep(10000);
    end = steady_clock::now();
  }

  atomic<bool> workload_done = false;
  size_t peak_tps = 0;
  std::thread display_thread([&]() {
    previous_completed_count_ = transactions_completed_count_.load();
    while (current_transaction_count_.load() > 0) {
      uint64_t snapshot_completed_count = transactions_completed_count_.load();
      uint64_t instantaneous_tps =
          snapshot_completed_count - previous_completed_count_;
      cout << "Reporting Origin: '" << reporting_origin
           << "' Remaining Transactions: '" << current_transaction_count_.load()
           << "' TPS: '" << instantaneous_tps << "'" << endl;
      previous_completed_count_ = snapshot_completed_count;
      peak_tps = std::max(peak_tps, instantaneous_tps);
      sleep_for(milliseconds(1000));
    }
    workload_done = true;
  });

  cout << "Duration passed, stopping" << endl;
  unique_lock<mutex> thread_lock(mutex_);
  is_running_ = false;
  condition_variable_.notify_all();
  thread_lock.unlock();

  cout << "Stopping working thread" << endl;
  if (working_thread_->joinable()) {
    working_thread_->join();
  }

  while (!workload_done.load()) {
    std::this_thread::yield();
  }

  auto closing_time = steady_clock::now();
  auto elapsed_time = duration_cast<seconds>(closing_time - begin);
  auto total_succeeded = total_executed_.load() - total_failed_.load();

  cout << "Workload is completed\n" << endl;
  cout << "Time Elapsed (Seconds): " << elapsed_time.count() << endl;
  cout << "Total Executed: " << total_executed_.load() << endl;
  cout << "\x1b[32mTotal Succeeded: " << total_succeeded << "\x1b[0m" << endl;
  if (total_failed_ > 0) {
    cout << "\x1b[31mTotal Failed: " << total_failed_.load() << " \x1b[0m"
         << endl;
  }
  if (total_succeeded > 0) {
    cout << "\x1b[33mTPS: "
         << total_executed_.load() / static_cast<double>(elapsed_time.count())
         << "\x1b[0m" << endl;
    cout << "\x1b[33mMax TPS: " << peak_tps << "\x1b[0m" << endl;
  }

  if (display_thread.joinable()) {
    display_thread.join();
  }
  execution_result = client->Stop();
  if (!execution_result.Successful()) {
    throw runtime_error("Cannot stop the client");
  }
}

void RunWithSingleClient(AppConfiguration& app_configuration,
                         shared_ptr<HttpClientInterface> http1_client,
                         shared_ptr<HttpClientInterface> http2_client,
                         shared_ptr<AsyncExecutorInterface> async_executor) {
  SingleCoordinatorConfig config;
  ReadSingleCoordinatorConfig(app_configuration.config_path, config);

  shared_ptr<TokenProviderCacheInterface> auth_token_provider_cache;
  if (app_configuration.cloud_platform_type == CloudPlatformType::AWS) {
    auth_token_provider_cache = GetAWSAuthTokenProviderCache(
        config.pbs_auth_endpoint, config.pbs_region, async_executor);
  } else if (app_configuration.cloud_platform_type == CloudPlatformType::GCP) {
    auth_token_provider_cache = GetGCPAuthTokenProviderCache(
        http1_client, config.pbs_auth_endpoint, async_executor);
  } else if (app_configuration.cloud_platform_type ==
             CloudPlatformType::Local) {
    auth_token_provider_cache = make_shared<DummyTokenProviderCache>();
  } else {
    throw runtime_error("Invalid platform type.");
  }

  assert(auth_token_provider_cache->Init().Successful());
  assert(auth_token_provider_cache->Run().Successful());

  auto client = make_shared<PrivacyBudgetServiceTransactionalClient>(
      config.reporting_origin, config.pbs_endpoint, http2_client,
      async_executor, auth_token_provider_cache);

  cout << "Running the workload against a single PBS" << endl;
  RunWorkload(config.reporting_origin, app_configuration, client);

  assert(auth_token_provider_cache->Stop().Successful());
}

void RunWithPBSTransactionalClient(
    const AppConfiguration& app_configuration,
    const shared_ptr<HttpClientInterface>& http1_client,
    const shared_ptr<HttpClientInterface>& http2_client,
    const shared_ptr<AsyncExecutorInterface>& async_executor) {
  MultiCoordinatorConfig config;
  ReadMultiCoordinatorConfig(app_configuration.config_path, config);

  shared_ptr<TokenProviderCacheInterface> auth_token_provider_cache_1;
  shared_ptr<TokenProviderCacheInterface> auth_token_provider_cache_2;

  if (app_configuration.cloud_platform_type == CloudPlatformType::AWS) {
    auth_token_provider_cache_1 = GetAWSAuthTokenProviderCache(
        config.pbs1_auth_endpoint, config.pbs1_region, async_executor);
    auth_token_provider_cache_2 = GetAWSAuthTokenProviderCache(
        config.pbs2_auth_endpoint, config.pbs2_region, async_executor);
  } else if (app_configuration.cloud_platform_type == CloudPlatformType::GCP) {
    auth_token_provider_cache_1 = GetGCPAuthTokenProviderCache(
        http1_client, config.pbs1_auth_endpoint, async_executor);
    auth_token_provider_cache_2 = GetGCPAuthTokenProviderCache(
        http1_client, config.pbs2_auth_endpoint, async_executor);
  } else if (app_configuration.cloud_platform_type ==
             CloudPlatformType::Local) {
    auth_token_provider_cache_1 = make_shared<DummyTokenProviderCache>();
    auth_token_provider_cache_2 = make_shared<DummyTokenProviderCache>();
  } else {
    throw runtime_error("Invalid platform type.");
  }

  assert(auth_token_provider_cache_1->Init().Successful());
  assert(auth_token_provider_cache_2->Init().Successful());
  assert(auth_token_provider_cache_1->Run().Successful());
  assert(auth_token_provider_cache_2->Run().Successful());

  auto client = make_shared<PrivacyBudgetServiceTransactionalClient>(
      config.reporting_origin, config.pbs1_endpoint, config.pbs2_endpoint,
      http2_client, async_executor, auth_token_provider_cache_1,
      auth_token_provider_cache_2);

  cout << "Running the workload against a multi PBS" << endl;
  RunWorkload(config.reporting_origin, app_configuration, client);

  assert(auth_token_provider_cache_1->Stop().Successful());
  assert(auth_token_provider_cache_2->Stop().Successful());
}
}  // namespace google::scp::pbs

void PrintHelp() {
  cout << "To use the workload generator, you need to update the config file "
          "next to the executable."
       << std::endl;
  cout << "ex: pbs_workload_generator single/multi config.json "
          "number_of_transactions "
          "number_of_unique_keys "
          "for_how_long_in_seconds "
          "cloud_platform_type i.e. aws/gcp/local"
       << std::endl;
}

void StartLogger() {
  unique_ptr<LoggerInterface> logger_ptr =
      make_unique<Logger>(make_unique<SyslogLogProvider>());
  if (!logger_ptr->Init().Successful()) {
    throw runtime_error("Cannot initialize logger.");
  }
  if (!logger_ptr->Run().Successful()) {
    throw runtime_error("Cannot run logger.");
  }
  GlobalLogger::SetGlobalLogger(std::move(logger_ptr));
}

using google::scp::pbs::AppConfiguration;
using google::scp::pbs::CloudPlatformType;

int main(int argc, char** argv) {
  SDKOptions aws_options;
  StartLogger();
  if (argc != 7) {
    PrintHelp();
    throw runtime_error("Invalid number of arguments.");
  }

  AppConfiguration app_configuration;

  char* end = NULL;
  app_configuration.total_transactions = strtol(argv[3], &end, 10);
  end = NULL;
  app_configuration.keys_per_transaction = strtol(argv[4], &end, 10);
  end = NULL;
  app_configuration.duration_in_seconds = strtol(argv[5], &end, 10);
  app_configuration.config_path = argv[2];
  app_configuration.cloud_platform_type = CloudPlatformType::Invalid;

  if (strcmp(argv[6], "aws") == 0) {
    cout << "Platform Type is AWS" << endl;
    app_configuration.cloud_platform_type = CloudPlatformType::AWS;
  } else if (strcmp(argv[6], "gcp") == 0) {
    cout << "Platform Type is GCP" << endl;
    app_configuration.cloud_platform_type = CloudPlatformType::GCP;
  } else if (strcmp(argv[6], "local") == 0) {
    cout << "Platform Type is Local" << endl;
    app_configuration.cloud_platform_type = CloudPlatformType::Local;
  } else {
    throw runtime_error("Invalid Platform Type.");
  }

  EnvConfigProvider config_provider;
  size_t http_request_max_retries_count;
  auto result = config_provider.Get(
      google::scp::pbs::kPBSWorkloadGeneratorMaxHttpRetryCount,
      http_request_max_retries_count);
  if (!result.Successful()) {
    http_request_max_retries_count = kHttp2RequestRetryStrategyMaxRetries;
  }

  cout << "Config path: " << app_configuration.config_path << endl;
  cout << "Total Txns: " << app_configuration.total_transactions << endl;
  cout << "Keys Per Txn: " << app_configuration.keys_per_transaction << endl;
  cout << "Duration in Seconds: " << app_configuration.duration_in_seconds
       << endl;

  size_t async_executor_thread_count = std::thread::hardware_concurrency() * 2;
  size_t async_executor_queue_cap = 100000;
  bool drop_tasks_on_close = true;
  shared_ptr<google::scp::core::AsyncExecutorInterface> async_executor =
      make_shared<google::scp::core::AsyncExecutor>(async_executor_thread_count,
                                                    async_executor_queue_cap,
                                                    drop_tasks_on_close);

  auto http1_client =
      make_shared<Http1CurlClient>(async_executor, async_executor);

  // We allow a number of connections per host to be the number of threads on
  // our async executor to eliminate connections being a bottleneck.
  auto http2_client = make_shared<HttpClient>(
      async_executor,
      HttpClientOptions(
          RetryStrategyOptions(RetryStrategyType::Exponential,
                               kDefaultRetryStrategyDelayInMs,
                               http_request_max_retries_count),
          async_executor_thread_count /*max_connections_per_host*/,
          kHttp2ConnectionReadTimeoutInSeconds));
  shared_ptr<TokenProviderCacheInterface> auth_token_provider_cache;

  assert(async_executor->Init().Successful());
  assert(http1_client->Init().Successful());
  assert(http2_client->Init().Successful());

  assert(async_executor->Run().Successful());
  assert(http1_client->Run().Successful());
  assert(http2_client->Run().Successful());

  // AWS needs SDK initialization.
  if (app_configuration.cloud_platform_type == CloudPlatformType::AWS) {
    InitAPI(aws_options);
  }

  if (strcmp(argv[1], "single") == 0) {
    RunWithSingleClient(app_configuration, http1_client, http2_client,
                        async_executor);
  } else if (strcmp(argv[1], "multi") == 0) {
    RunWithPBSTransactionalClient(app_configuration, http1_client, http2_client,
                                  async_executor);
  } else {
    PrintHelp();
    throw runtime_error("Invalid coordinator type.");
  }

  if (app_configuration.cloud_platform_type == CloudPlatformType::AWS) {
    ShutdownAPI(aws_options);
  }

  assert(http2_client->Stop().Successful());
  assert(http1_client->Stop().Successful());
  assert(async_executor->Stop().Successful());
}
