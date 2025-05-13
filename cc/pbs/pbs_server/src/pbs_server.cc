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
#include <list>
#include <memory>
#include <string>
#include <unordered_set>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/log/check.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/config_provider/src/env_config_provider.h"
#include "cc/core/logger/src/log_providers/stdout/stdout_log_provider.h"
#include "cc/core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "cc/core/logger/src/log_utils.h"
#include "cc/core/logger/src/logger.h"
#include "cc/pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_v3.h"
#include "cc/public/core/interface/execution_result.h"

#if defined(PBS_GCP)
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/gcp_dependency_factory.h"
#elif defined(PBS_LOCAL)
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"
#endif

namespace {

using ::privacy_sandbox::pbs::CloudPlatformDependencyFactoryInterface;
using ::privacy_sandbox::pbs::PBSInstanceV3;
using ::privacy_sandbox::pbs_common::ConfigProviderInterface;
using ::privacy_sandbox::pbs_common::EnvConfigProvider;
using ::privacy_sandbox::pbs_common::ExecutionResultOr;
using ::privacy_sandbox::pbs_common::GlobalLogger;
using ::privacy_sandbox::pbs_common::kZeroUuid;
using ::privacy_sandbox::pbs_common::Logger;
using ::privacy_sandbox::pbs_common::LoggerInterface;
using ::privacy_sandbox::pbs_common::LogLevel;
using ::privacy_sandbox::pbs_common::LogLevelFromString;
using ::privacy_sandbox::pbs_common::ServiceInterface;
using ::privacy_sandbox::pbs_common::StdoutLogProvider;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using ::privacy_sandbox::pbs_common::SyslogLogProvider;

std::shared_ptr<ConfigProviderInterface> config_provider;
std::shared_ptr<ServiceInterface> pbs_instance;

inline constexpr char kPBSServer[] = "PBSServer";
inline constexpr char kStdoutLogProvider[] = "StdoutLogProvider";

inline ExecutionResultOr<
    std::unique_ptr<CloudPlatformDependencyFactoryInterface>>
GetEnvironmentSpecificFactory(const std::shared_ptr<ConfigProviderInterface>&
                                  config_provider_for_factory) {
#if defined(PBS_GCP)
  SCP_INFO(kPBSServer, kZeroUuid, "Running GCP PBS.");
  return std::make_unique<privacy_sandbox::pbs::GcpDependencyFactory>(
      config_provider_for_factory);
#elif defined(PBS_LOCAL)
  SCP_INFO(kPBSServer, kZeroUuid, "Running Local PBS.");
  return std::make_unique<privacy_sandbox::pbs::LocalDependencyFactory>(
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
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result,
              absl::StrFormat("%s", err_message.c_str()));
    throw std::runtime_error(err_message);
  }

  SCP_INFO(kPBSServer, kZeroUuid, "Properly initialized the service.");
}

void Run(std::shared_ptr<ServiceInterface> service, std::string service_name) {
  auto execution_result = service->Run();
  if (!execution_result.Successful()) {
    auto err_message = service_name + " failed to run.";
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result,
              absl::StrFormat("%s", err_message.c_str()));
    throw std::runtime_error(err_message);
  }

  SCP_INFO(kPBSServer, kZeroUuid, "Properly run the service.");
}

void Stop(std::shared_ptr<ServiceInterface> service, std::string service_name) {
  auto execution_result = service->Stop();
  if (!execution_result.Successful()) {
    auto err_message = service_name + " failed to stop.";
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result,
              absl::StrFormat("%s", err_message.c_str()));
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
          ->Get(privacy_sandbox::pbs::kEnabledLogLevels, enabled_log_levels)
          .Successful()) {
    for (auto enabled_log_level : enabled_log_levels) {
      log_levels.emplace(LogLevelFromString(enabled_log_level));
    }

    GlobalLogger::SetGlobalLogLevels(log_levels);
  }

  std::unique_ptr<LoggerInterface> logger_ptr;
  if (std::string log_provider;
      config_provider->Get(privacy_sandbox::pbs::kLogProvider, log_provider)
          .Successful() &&
      log_provider == kStdoutLogProvider) {
    logger_ptr =
        std::make_unique<Logger>(std::make_unique<StdoutLogProvider>());
  } else {
    logger_ptr =
        std::make_unique<Logger>(std::make_unique<SyslogLogProvider>());
  }
  if (!logger_ptr->Init().Successful()) {
    throw std::runtime_error("Cannot initialize logger.");
  }
  if (!logger_ptr->Run().Successful()) {
    throw std::runtime_error("Cannot run logger.");
  }
  GlobalLogger::SetGlobalLogger(std::move(logger_ptr));

  SCP_INFO(kPBSServer, kZeroUuid, "Instantiating PBSInstanceV3.");
  auto factory_interface = GetEnvironmentSpecificFactory(config_provider);
  CHECK(factory_interface.Successful())
      << "GetEnvironmentSpecificFactory was unsuccessful.";
  pbs_instance = std::make_shared<PBSInstanceV3>(
      config_provider, std::move(factory_interface.value()));

  Init(pbs_instance, "PBS_Instance");
  Run(pbs_instance, "PBS_Instance");

  while (true) {
    std::this_thread::sleep_for(std::chrono::minutes(1));
  }
  return 0;
}
