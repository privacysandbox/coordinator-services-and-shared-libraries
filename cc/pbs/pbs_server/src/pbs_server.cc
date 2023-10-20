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

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>

#include "absl/debugging/failure_signal_handler.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/config_provider/src/env_config_provider.h"
#include "core/interface/errors.h"
#include "core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "core/logger/src/log_utils.h"
#include "core/logger/src/logger.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance_multi_partition_platform_wrapper.h"

#include "error_codes.h"

using google::scp::core::ConfigProviderInterface;
using google::scp::core::EnvConfigProvider;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LoggerInterface;
using google::scp::core::LogLevel;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::FromString;
using google::scp::core::logger::Logger;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::pbs::PBSInstance;
using google::scp::pbs::PBSInstanceConfig;
using google::scp::pbs::PBSInstanceMultiPartitionPlatformWrapper;
using std::list;
using std::make_shared;
using std::make_unique;
using std::mutex;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_ptr;
using std::unordered_set;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::filesystem::exists;
using std::filesystem::path;

shared_ptr<ConfigProviderInterface> config_provider;
shared_ptr<ServiceInterface> pbs_instance;

static constexpr char kPBSServer[] = "PBSServer";

void Init(shared_ptr<ServiceInterface> service, string service_name) {
  auto execution_result = service->Init();
  if (!execution_result.Successful()) {
    auto err_message = service_name + " failed to initialized.";
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result, "%s",
              err_message.c_str());
    throw runtime_error(err_message);
  }

  SCP_INFO(kPBSServer, kZeroUuid, "Properly initialized the service.");
}

void Run(shared_ptr<ServiceInterface> service, string service_name) {
  auto execution_result = service->Run();
  if (!execution_result.Successful()) {
    auto err_message = service_name + " failed to run.";
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result, "%s",
              err_message.c_str());
    throw runtime_error(err_message);
  }

  SCP_INFO(kPBSServer, kZeroUuid, "Properly run the service.");
}

void Stop(shared_ptr<ServiceInterface> service, string service_name) {
  auto execution_result = service->Stop();
  if (!execution_result.Successful()) {
    auto err_message = service_name + " failed to stop.";
    SCP_ERROR(kPBSServer, kZeroUuid, execution_result, "%s",
              err_message.c_str());
    throw runtime_error(err_message);
  }

  SCP_INFO(kPBSServer, kZeroUuid, "Properly stopped the service.");
}

std::string ReadConfig(const string& config_key) {
  string config_value;
  if (config_provider->Get(config_key, config_value) !=
      SuccessExecutionResult()) {
    throw runtime_error(config_key + " is not provided");
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

  config_provider = make_shared<EnvConfigProvider>();
  config_provider->Init();

  list<string> enabled_log_levels;
  unordered_set<LogLevel> log_levels;
  if (config_provider
          ->Get(google::scp::pbs::kEnabledLogLevels, enabled_log_levels)
          .Successful()) {
    for (auto enabled_log_level : enabled_log_levels) {
      log_levels.emplace(FromString(enabled_log_level));
    }

    GlobalLogger::SetGlobalLogLevels(log_levels);
  }

  unique_ptr<LoggerInterface> logger_ptr =
      make_unique<Logger>(make_unique<SyslogLogProvider>());
  if (!logger_ptr->Init().Successful()) {
    throw runtime_error("Cannot initialize logger.");
  }
  if (!logger_ptr->Run().Successful()) {
    throw runtime_error("Cannot run logger.");
  }
  GlobalLogger::SetGlobalLogger(std::move(logger_ptr));

  bool pbs_partitioning_enabled = false;
  if (config_provider
          ->Get(google::scp::pbs::kPBSPartitioningEnabled,
                pbs_partitioning_enabled)
          .Successful() &&
      pbs_partitioning_enabled) {
    SCP_INFO(kPBSServer, kZeroUuid, "Instantiated Multi-Partition PBS");
    pbs_instance =
        make_shared<PBSInstanceMultiPartitionPlatformWrapper>(config_provider);
  } else {
    SCP_INFO(kPBSServer, kZeroUuid,
             "Instantiated Single-Partition (Global Partition) PBS");
    pbs_instance = make_shared<PBSInstance>(config_provider);
  }

  Init(pbs_instance, "PBS_Instance");
  Run(pbs_instance, "PBS_Instance");

  while (true) {
    std::this_thread::sleep_for(minutes(1));
  }
  return 0;
}
