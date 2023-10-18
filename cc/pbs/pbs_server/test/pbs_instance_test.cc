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

#include "pbs/pbs_server/src/pbs_instance/pbs_instance.h"

#include <gtest/gtest.h>

#include <stdlib.h>

#include <filesystem>
#include <string>

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/http2_client/src/http2_client.h"
#include "cc/pbs/pbs_client/src/pbs_client.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/config_provider/src/env_config_provider.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/lease_manager_interface.h"
#include "core/lease_manager/src/lease_manager.h"
#include "core/tcp_traffic_forwarder/mock/mock_traffic_forwarder.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/leasable_lock/mock/mock_leasable_lock.h"
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"
#include "pbs/pbs_server/src/pbs_instance/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::EnvConfigProvider;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpClientOptions;
using google::scp::core::LeasableLockInterface;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManager;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::tcp_traffic_forwarder::mock::MockTCPTrafficForwarder;
using google::scp::core::test::ResultIs;
using google::scp::core::test::TestLoggingUtils;
using google::scp::core::test::WaitUntil;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::pbs::PBSInstance;
using google::scp::pbs::leasable_lock::mock::MockLeasableLock;
using std::atomic;
using std::make_shared;
using std::mutex;
using std::optional;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_lock;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::pbs::test {

class PBSInstancePrivateTester : public PBSInstance {
 public:
  PBSInstancePrivateTester(
      shared_ptr<ConfigProviderInterface> config_provider = nullptr)
      : PBSInstance(config_provider) {}

  void TestRunLeaseManagerAndWaitUntilLeaseIsAcquired() {
    atomic<bool> is_terminated = false;
    auto terminate_function = [&is_terminated]() { is_terminated = true; };

    auto lease_manager =
        make_shared<LeaseManager>(100 /* lease enforcer periodicity */,
                                  3000 /* lease obtainer max running time */);
    EXPECT_SUCCESS(lease_manager->Init());

    auto traffic_forwarder = make_shared<MockTCPTrafficForwarder>();
    EXPECT_EQ(traffic_forwarder->GetForwardingAddress(), "");

    // Some lease holder exists with "1.1.1.1"
    auto leasable_lock =
        make_shared<MockLeasableLock>(500 /* lease duration */);
    leasable_lock->should_refresh_lease_ = true;
    leasable_lock->is_owner_ = false;
    leasable_lock->SetCurrentLeaseOwnerInfo({"123445", "1.1.1.1"});

    atomic<bool> wait_finished = false;
    auto thread_obj = thread([=, &wait_finished]() {
      RunLeaseManagerAndWaitUntilLeaseIsAcquired(
          lease_manager, leasable_lock, traffic_forwarder, terminate_function);
      wait_finished = true;
    });

    // Waiting should not finish
    sleep_for(milliseconds(1000));
    EXPECT_EQ(wait_finished.load(), false);

    // Wait for forwarding address to be updated.
    while (traffic_forwarder->GetForwardingAddress() == "") {
      sleep_for(milliseconds(40));
    }
    EXPECT_EQ(traffic_forwarder->GetForwardingAddress(), "1.1.1.1");
    EXPECT_EQ(wait_finished.load(), false);

    leasable_lock->should_refresh_lease_ = true;
    leasable_lock->is_owner_ = true;
    leasable_lock->SetCurrentLeaseOwnerInfo({"222222", "2.2.2.2"});

    // Waiting should now finish
    while (!wait_finished) {
      sleep_for(milliseconds(40));
    }

    // Changing the lease owner causes the termination
    leasable_lock->should_refresh_lease_ = true;
    leasable_lock->is_owner_ = false;
    leasable_lock->SetCurrentLeaseOwnerInfo({"123445", "1.1.1.1"});

    while (!is_terminated) {
      sleep_for(milliseconds(40));
    }

    if (thread_obj.joinable()) {
      thread_obj.join();
    }

    EXPECT_SUCCESS(lease_manager->Stop());
  }
};

TEST(PBSInstanceTest, TestRunLeaseManagerAndWaitUntilLeaseIsAcquired) {
  PBSInstancePrivateTester tester;
  tester.TestRunLeaseManagerAndWaitUntilLeaseIsAcquired();
}

}  // namespace google::scp::pbs::test
