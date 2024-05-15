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

#include <sys/wait.h>

#include <chrono>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/log/check.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/config_provider/src/env_config_provider.h"
#include "cc/core/interface/errors.h"
#include "cc/core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "cc/core/logger/src/log_utils.h"
#include "cc/core/logger/src/logger.h"
#include "cc/pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/pbs_server/src/error_codes.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_multi_partition_platform_wrapper.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_v3.h"
#include "cc/public/core/interface/execution_result.h"

#if defined(PBS_GCP)
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/gcp_dependency_factory.h"
#elif defined(PBS_GCP_INTEGRATION_TEST)
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp_integration_test/gcp_integration_test_dependency_factory.h"
#elif defined(PBS_AWS)
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/aws/aws_dependency_factory.h"
#elif defined(PBS_AWS_INTEGRATION_TEST)
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/aws_integration_test/aws_integration_test_dependency_factory.h"
#elif defined(PBS_LOCAL)
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"
#endif

namespace {

using ::google::scp::core::ConfigProviderInterface;
using ::google::scp::core::EnvConfigProvider;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::ExecutionResultOr;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::LoggerInterface;
using ::google::scp::core::LogLevel;
using ::google::scp::core::ServiceInterface;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::common::GlobalLogger;
using ::google::scp::core::common::kZeroUuid;
using ::google::scp::core::errors::GetErrorMessage;
using ::google::scp::core::errors::INVALID_ENVIROMENT;
using ::google::scp::core::logger::FromString;
using ::google::scp::core::logger::Logger;
using ::google::scp::core::logger::log_providers::SyslogLogProvider;
using ::google::scp::pbs::CloudPlatformDependencyFactoryInterface;
using ::google::scp::pbs::PBSInstance;
using ::google::scp::pbs::PBSInstanceMultiPartitionPlatformWrapper;
using ::google::scp::pbs::PBSInstanceV3;

std::shared_ptr<ConfigProviderInterface> config_provider;
std::shared_ptr<ServiceInterface> pbs_instance;

inline constexpr char kPBSServer[] = "PBSServer";

inline ExecutionResultOr<
    std::unique_ptr<CloudPlatformDependencyFactoryInterface>>
GetEnvironmentSpecificFactory(const std::shared_ptr<ConfigProviderInterface>&
                                  config_provider_for_factory) {
#if defined(PBS_GCP)
  SCP_INFO(kPBSServer, kZeroUuid, "Running GCP PBS.");
  return std::make_unique<google::scp::pbs::GcpDependencyFactory>(
      config_provider_for_factory);
#elif defined(PBS_GCP_INTEGRATION_TEST)
  SCP_INFO(kPBSServer, kZeroUuid, "Running GCP Integration Test PBS.");
  return std::make_unique<
      google::scp::pbs::GcpIntegrationTestDependencyFactory>(
      config_provider_for_factory);
#elif defined(PBS_AWS)
  SCP_INFO(kPBSServer, kZeroUuid, "Running AWS PBS.");
  return std::make_unique<google::scp::pbs::AwsDependencyFactory>(
      config_provider_for_factory);
#elif defined(PBS_AWS_INTEGRATION_TEST)
  SCP_INFO(kPBSServer, kZeroUuid, "Running AWS Integration Test PBS.");
  return std::make_unique<
      google::scp::pbs::AwsIntegrationTestDependencyFactory>(
      config_provider_for_factory);
#elif defined(PBS_LOCAL)
  SCP_INFO(kPBSServer, kZeroUuid, "Running Local PBS.");
  return std::make_unique<google::scp::pbs::LocalDependencyFactory>(
      config_provider_for_factory);
#else
  SCP_INFO(kPBSServer, kZeroUuid, "Environment not found.");
  return FailureExecutionResult(INVALID_ENVIROMENT);
#endif
}

}  // namespace

void Init(std::shared_ptr<ServiceInterface> service, std::string service_name) {
  auto execution_result = service->Init();
  if (!execution_result.Successful()) {
    auto err_message = service_name + " failed to initialized.";
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result, "%s",
              err_message.c_str());
    throw std::runtime_error(err_message);
  }

  SCP_INFO(kPBSServer, kZeroUuid, "Properly initialized the service.");
}

void Run(std::shared_ptr<ServiceInterface> service, std::string service_name) {
  auto execution_result = service->Run();
  if (!execution_result.Successful()) {
    auto err_message = service_name + " failed to run.";
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result, "%s",
              err_message.c_str());
    throw std::runtime_error(err_message);
  }

  SCP_INFO(kPBSServer, kZeroUuid, "Properly run the service.");
}

void Stop(std::shared_ptr<ServiceInterface> service, std::string service_name) {
  auto execution_result = service->Stop();
  if (!execution_result.Successful()) {
    auto err_message = service_name + " failed to stop.";
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result, "%s",
              err_message.c_str());
    throw std::runtime_error(err_message);
  }

  SCP_INFO(kPBSServer, kZeroUuid, "Properly stopped the service.");
}

std::string ReadConfig(const std::string& config_key) {
  std::string config_value;
  if (config_provider->Get(config_key, config_value) !=
      SuccessExecutionResult()) {
    throw std::runtime_error(config_key + " is not provided");
  }
  return config_value;
}

// PBS can start other processes. In order to make sure these processes are
// cleaned correctly upon their exiting, we need to waitpid them.
static void SigChildHandler(int sig) {
  pid_t pid;
  int status;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {}
}

int main(int argc, char** argv) {
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, &SigChildHandler);
  signal(SIGHUP, SIG_IGN);

  // https://github.com/abseil/abseil-cpp/blob/master/absl/debugging/failure_signal_handler.h
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  config_provider = std::make_shared<EnvConfigProvider>();
  config_provider->Init();

  std::list<std::string> enabled_log_levels;
  std::unordered_set<LogLevel> log_levels;
  if (config_provider
          ->Get(google::scp::pbs::kEnabledLogLevels, enabled_log_levels)
          .Successful()) {
    for (auto enabled_log_level : enabled_log_levels) {
      log_levels.emplace(FromString(enabled_log_level));
    }

    GlobalLogger::SetGlobalLogLevels(log_levels);
  }

  std::unique_ptr<LoggerInterface> logger_ptr =
      std::make_unique<Logger>(std::make_unique<SyslogLogProvider>());
  if (!logger_ptr->Init().Successful()) {
    throw std::runtime_error("Cannot initialize logger.");
  }
  if (!logger_ptr->Run().Successful()) {
    throw std::runtime_error("Cannot run logger.");
  }
  GlobalLogger::SetGlobalLogger(std::move(logger_ptr));

  bool pbs_partitioning_enabled = false;
  bool pbs_relaxed_consistency_enabled = false;
  if (config_provider
          ->Get(google::scp::pbs::kPBSPartitioningEnabled,
                pbs_partitioning_enabled)
          .Successful() &&
      pbs_partitioning_enabled) {
    SCP_INFO(kPBSServer, kZeroUuid, "Instantiated Multi-Partition PBS");
    pbs_instance = std::make_shared<PBSInstanceMultiPartitionPlatformWrapper>(
        config_provider);
  } else if (config_provider
                 ->Get(google::scp::pbs::kPBSRelaxedConsistencyEnabled,
                       pbs_relaxed_consistency_enabled)
                 .Successful() &&
             pbs_relaxed_consistency_enabled) {
    SCP_INFO(kPBSServer, kZeroUuid, "Instantiating PBSInstanceV3.");
    auto factory_interface = GetEnvironmentSpecificFactory(config_provider);
    CHECK(factory_interface.Successful())
        << "GetEnvironmentSpecificFactory was unsuccessful.";
    pbs_instance = std::make_shared<PBSInstanceV3>(
        config_provider, std::move(factory_interface.value()));
  } else {
    SCP_INFO(kPBSServer, kZeroUuid,
             "Instantiated Single-Partition (Global Partition) PBS");
    pbs_instance = std::make_shared<PBSInstance>(config_provider);
  }

  Init(pbs_instance, "PBS_Instance");
  Run(pbs_instance, "PBS_Instance");

  while (true) {
    std::this_thread::sleep_for(std::chrono::minutes(1));
  }
  return 0;
}
