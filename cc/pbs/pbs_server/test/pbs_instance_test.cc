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

#include <string>

#include "core/config_provider/src/env_config_provider.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/lease_manager_interface.h"
#include "core/lease_manager/src/lease_manager.h"
#include "core/tcp_traffic_forwarder/mock/mock_traffic_forwarder.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/pbs_server/src/pbs_instance/error_codes.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::ConfigProviderInterface;
using google::scp::core::EnvConfigProvider;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LeasableLockInterface;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManager;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::errors::SC_PBS_INVALID_HTTP2_SERVER_CERT_FILE_PATH;
using google::scp::core::errors::
    SC_PBS_INVALID_HTTP2_SERVER_PRIVATE_KEY_FILE_PATH;
using google::scp::core::tcp_traffic_forwarder::mock::MockTCPTrafficForwarder;
using google::scp::pbs::PBSInstance;
using std::atomic;
using std::make_shared;
using std::mutex;
using std::optional;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_lock;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::pbs::test {

class MockLeasableLock : public LeasableLockInterface {
 public:
  explicit MockLeasableLock(TimeDuration lease_duration)
      : lease_duration_(lease_duration) {}

  bool ShouldRefreshLease() const noexcept override {
    return should_refresh_lease_;
  }

  ExecutionResult RefreshLease() noexcept override {
    return SuccessExecutionResult();
  }

  TimeDuration GetConfiguredLeaseDurationInMilliseconds()
      const noexcept override {
    return lease_duration_;
  }

  optional<LeaseInfo> GetCurrentLeaseOwnerInfo() const noexcept override {
    unique_lock lock(mutex_);
    return current_lease_owner_info_;
  }

  void SetCurrentLeaseOwnerInfo(LeaseInfo lease_info) noexcept {
    unique_lock lock(mutex_);
    current_lease_owner_info_ = lease_info;
  }

  bool IsCurrentLeaseOwner() const noexcept override { return is_owner_; }

  mutable mutex mutex_;
  LeaseInfo current_lease_owner_info_ = {};
  atomic<bool> is_owner_ = false;
  TimeDuration lease_duration_;
  atomic<bool> should_refresh_lease_ = true;
};

class PBSInstancePrivateTester : public PBSInstance {
 public:
  PBSInstancePrivateTester(
      shared_ptr<ConfigProviderInterface> config_provider = nullptr)
      : PBSInstance(config_provider) {}

  ExecutionResult ReadConfiguration() {
    return PBSInstance::ReadConfigurations();
  }

  PBSInstanceConfig GetInstanceConfig() { return pbs_instance_config_; }

  void TestRunLeaseManagerAndWaitUntilLeaseIsAcquired() {
    atomic<bool> is_terminated = false;
    auto terminate_function = [&is_terminated]() { is_terminated = true; };

    auto lease_manager =
        make_shared<LeaseManager>(100 /* lease enforcer periodicity */,
                                  3000 /* lease obtainer max running time */);
    EXPECT_EQ(lease_manager->Init(), SuccessExecutionResult());

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

    EXPECT_EQ(lease_manager->Stop(), SuccessExecutionResult());
  }
};

TEST(PBSInstanceTest, TestRunLeaseManagerAndWaitUntilLeaseIsAcquired) {
  PBSInstancePrivateTester tester;
  tester.TestRunLeaseManagerAndWaitUntilLeaseIsAcquired();
}

static void SetAllConfigs() {
  setenv(kAsyncExecutorQueueSize, "1", 1);
  setenv(kAsyncExecutorThreadsCount, "1", 1);
  setenv(kIOAsyncExecutorQueueSize, "1", 1);
  setenv(kIOAsyncExecutorThreadsCount, "1", 1);
  setenv(kTransactionManagerCapacity, "1", 1);
  setenv(kJournalServiceBucketName, "name", 1);
  setenv(kJournalServicePartitionName, "part", 1);
  setenv(kPrivacyBudgetServiceHostAddress, "0.0.0.0", 1);
  setenv(kPrivacyBudgetServiceHostPort, "8000", 1);
  setenv(kPrivacyBudgetServiceHealthPort, "8001", 1);
  setenv(kRemotePrivacyBudgetServiceCloudServiceRegion, "region", 1);
  setenv(kRemotePrivacyBudgetServiceAuthServiceEndpoint, "https://auth.com", 1);
  setenv(kRemotePrivacyBudgetServiceClaimedIdentity, "remote-id", 1);
  setenv(kRemotePrivacyBudgetServiceAssumeRoleArn, "arn", 1);
  setenv(kRemotePrivacyBudgetServiceAssumeRoleExternalId, "id", 1);
  setenv(kRemotePrivacyBudgetServiceHostAddress, "https://remote.com", 1);
  setenv(kAuthServiceEndpoint, "https://auth.com", 1);
  setenv(core::kCloudServiceRegion, "region", 1);
  setenv(kServiceMetricsNamespace, "ns", 1);
  setenv(kTotalHttp2ServerThreadsCount, "1", 1);

  unsetenv(kHttp2ServerUseTls);
  unsetenv(kHttp2ServerPrivateKeyFilePath);
  unsetenv(kHttp2ServerCertificateFilePath);
}

TEST(PBSInstanceTest, ReadConfigurationShouldFailIfUseTlsButNoPrivateKeyPath) {
  shared_ptr<ConfigProviderInterface> config_prover =
      make_shared<EnvConfigProvider>();
  PBSInstancePrivateTester tester(config_prover);

  SetAllConfigs();

  setenv(kHttp2ServerUseTls, "true", 1);
  setenv(kHttp2ServerCertificateFilePath, "/cert/path", 1);
  // Error if unset
  EXPECT_EQ(tester.ReadConfiguration(),
            FailureExecutionResult(
                SC_PBS_INVALID_HTTP2_SERVER_PRIVATE_KEY_FILE_PATH));
  // Error if empty
  setenv(kHttp2ServerPrivateKeyFilePath, "", 1);
  EXPECT_EQ(tester.ReadConfiguration(),
            FailureExecutionResult(
                SC_PBS_INVALID_HTTP2_SERVER_PRIVATE_KEY_FILE_PATH));
}

TEST(PBSInstanceTest, ReadConfigurationShouldFailIfUseTlsButNoCertificatePath) {
  shared_ptr<ConfigProviderInterface> config_prover =
      make_shared<EnvConfigProvider>();
  PBSInstancePrivateTester tester(config_prover);

  SetAllConfigs();
  setenv(kHttp2ServerUseTls, "true", 1);
  setenv(kHttp2ServerPrivateKeyFilePath, "/key/path", 1);
  // Error if unset
  EXPECT_EQ(tester.ReadConfiguration(),
            FailureExecutionResult(SC_PBS_INVALID_HTTP2_SERVER_CERT_FILE_PATH));

  setenv(kHttp2ServerCertificateFilePath, "", 1);
  // Error if empty
  EXPECT_EQ(tester.ReadConfiguration(),
            FailureExecutionResult(SC_PBS_INVALID_HTTP2_SERVER_CERT_FILE_PATH));
}

TEST(PBSInstanceTest,
     ReadConfigurationShouldSucceedIfUseTlsAndCertAndKeyPathsAreSet) {
  shared_ptr<ConfigProviderInterface> config_prover =
      make_shared<EnvConfigProvider>();
  PBSInstancePrivateTester tester(config_prover);

  SetAllConfigs();
  setenv(kHttp2ServerUseTls, "true", 1);
  setenv(kHttp2ServerPrivateKeyFilePath, "/key/path", 1);
  setenv(kHttp2ServerCertificateFilePath, "/cert/path", 1);

  EXPECT_TRUE(tester.ReadConfiguration().Successful());

  auto instance_config = tester.GetInstanceConfig();
  EXPECT_TRUE(instance_config.http2_server_use_tls);
  EXPECT_EQ(*instance_config.http2_server_private_key_file_path, "/key/path");
  EXPECT_EQ(*instance_config.http2_server_certificate_file_path, "/cert/path");
}

TEST(PBSInstanceTest, ReadConfigurationShouldSucceedIfMissingUseTlsOrEmpty) {
  shared_ptr<ConfigProviderInterface> config_prover =
      make_shared<EnvConfigProvider>();
  PBSInstancePrivateTester tester(config_prover);

  SetAllConfigs();
  // Missing use tls config
  EXPECT_TRUE(tester.ReadConfiguration().Successful());

  auto instance_config = tester.GetInstanceConfig();
  EXPECT_FALSE(instance_config.http2_server_use_tls);
  EXPECT_EQ(*instance_config.http2_server_private_key_file_path, "");
  EXPECT_EQ(*instance_config.http2_server_certificate_file_path, "");

  // Empty use tls config
  setenv(kHttp2ServerUseTls, "", 1);
  EXPECT_TRUE(tester.ReadConfiguration().Successful());

  instance_config = tester.GetInstanceConfig();
  EXPECT_FALSE(instance_config.http2_server_use_tls);
  EXPECT_EQ(*instance_config.http2_server_private_key_file_path, "");
  EXPECT_EQ(*instance_config.http2_server_certificate_file_path, "");
}

TEST(PBSInstanceTest, ReadConfigurationShouldSucceedIfUseTlsParsingFails) {
  shared_ptr<ConfigProviderInterface> config_prover =
      make_shared<EnvConfigProvider>();
  PBSInstancePrivateTester tester(config_prover);

  SetAllConfigs();
  // Does not parse to bool
  setenv(kHttp2ServerUseTls, "t", 1);

  EXPECT_TRUE(tester.ReadConfiguration().Successful());

  auto instance_config = tester.GetInstanceConfig();
  EXPECT_FALSE(instance_config.http2_server_use_tls);
  EXPECT_EQ(*instance_config.http2_server_private_key_file_path, "");
  EXPECT_EQ(*instance_config.http2_server_certificate_file_path, "");

  // Does not parse to bool
  setenv(kHttp2ServerUseTls, "123", 1);

  EXPECT_TRUE(tester.ReadConfiguration().Successful());

  instance_config = tester.GetInstanceConfig();
  EXPECT_FALSE(instance_config.http2_server_use_tls);
  EXPECT_EQ(*instance_config.http2_server_private_key_file_path, "");
  EXPECT_EQ(*instance_config.http2_server_certificate_file_path, "");
}

TEST(PBSInstanceTest, ReadConfigurationShouldSucceedIfUseTlsIsSetToFalse) {
  shared_ptr<ConfigProviderInterface> config_prover =
      make_shared<EnvConfigProvider>();
  PBSInstancePrivateTester tester(config_prover);

  SetAllConfigs();
  setenv(kHttp2ServerUseTls, "false", 1);

  EXPECT_TRUE(tester.ReadConfiguration().Successful());

  auto instance_config = tester.GetInstanceConfig();
  EXPECT_FALSE(instance_config.http2_server_use_tls);
  EXPECT_EQ(*instance_config.http2_server_private_key_file_path, "");
  EXPECT_EQ(*instance_config.http2_server_certificate_file_path, "");
}
}  // namespace google::scp::pbs::test
