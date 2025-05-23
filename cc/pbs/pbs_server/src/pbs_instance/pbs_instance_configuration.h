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

#pragma once

#include <memory>
#include <string>

#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/pbs_server/src/pbs_instance/pbs_instance_logging.h"
#include "cc/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace privacy_sandbox::pbs {
inline constexpr int kDefaultLeaseDurationInSeconds = 10;
inline constexpr char kComputeEngine[] = "compute_engine";

/**
 * PBS Instance Configuration Knobs.
 *
 * @brief This structure is intended to supply configurations that are directly
 * used by the PBSInstance/PBSInstancev2 classes in construction of top level
 * components. For configurations used within components that PBS instance
 * creates, please provide config provider to corresponding components during
 * construction time to allow the respective components to fetch their relevant
 * configurations. Any platform specific configurations should be read directly
 * inside the Platform Dependency Factory class using config provider.
 */
struct PBSInstanceConfig {
  size_t async_executor_queue_size = 100000;
  size_t async_executor_thread_pool_size = 16;
  size_t io_async_executor_queue_size = 100000;
  size_t io_async_executor_thread_pool_size = 2000;
  size_t http2server_thread_pool_size = 256;
  size_t async_executor_thread_pool_size_for_lease_db_requests = 2;
  size_t async_executor_queue_size_for_lease_db_requests = 10000;

  std::shared_ptr<std::string> host_address;
  std::shared_ptr<std::string> host_port;
  std::shared_ptr<std::string> health_port;

  // TLS context for HTTP2 Server
  bool http2_server_use_tls = false;
  std::shared_ptr<std::string> http2_server_private_key_file_path;
  std::shared_ptr<std::string> http2_server_certificate_file_path;
};

/**
 * @brief Extracts the PBS relevant configuration values from the config
 * provider and returns the PBSInstanceConfig.
 *
 * @param config_provider
 * @return pbs_common::ExecutionResultOr<PBSInstanceConfig>
 */
[[maybe_unused]] static pbs_common::ExecutionResultOr<PBSInstanceConfig>
GetPBSInstanceConfigFromConfigProvider(
    std::shared_ptr<pbs_common::ConfigProviderInterface>
        config_provider) noexcept {
  PBSInstanceConfig pbs_instance_config;
  auto execution_result = config_provider->Get(
      kAsyncExecutorQueueSize, pbs_instance_config.async_executor_queue_size);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kPBSInstance, pbs_common::kZeroUuid, execution_result,
                 "Failed to read async executor queue size.");
    return execution_result;
  }

  execution_result =
      config_provider->Get(kAsyncExecutorThreadsCount,
                           pbs_instance_config.async_executor_thread_pool_size);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kPBSInstance, pbs_common::kZeroUuid, execution_result,
                 "Failed to read async executor thread pool size.");
    return execution_result;
  }

  execution_result =
      config_provider->Get(kIOAsyncExecutorQueueSize,
                           pbs_instance_config.io_async_executor_queue_size);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kPBSInstance, pbs_common::kZeroUuid, execution_result,
                 "Failed to read io async executor queue size.");
    return execution_result;
  }

  execution_result = config_provider->Get(
      kIOAsyncExecutorThreadsCount,
      pbs_instance_config.io_async_executor_thread_pool_size);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kPBSInstance, pbs_common::kZeroUuid, execution_result,
                 "Failed to read io async executor thread pool size.");
    return execution_result;
  }

  pbs_instance_config.host_address = std::make_shared<std::string>();
  execution_result = config_provider->Get(kPrivacyBudgetServiceHostAddress,
                                          *pbs_instance_config.host_address);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kPBSInstance, pbs_common::kZeroUuid, execution_result,
                 "Failed to read host address.");
    return execution_result;
  }

  pbs_instance_config.host_port = std::make_shared<std::string>();
  execution_result = config_provider->Get(kPrivacyBudgetServiceHostPort,
                                          *pbs_instance_config.host_port);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kPBSInstance, pbs_common::kZeroUuid, execution_result,
                 "Failed to read host port.");
    return execution_result;
  }

  if (std::string container_type;
      !config_provider->Get(kContainerType, container_type).Successful() ||
      container_type == kComputeEngine) {
    pbs_instance_config.health_port = std::make_shared<std::string>();
    execution_result = config_provider->Get(kPrivacyBudgetServiceHealthPort,
                                            *pbs_instance_config.health_port);
    if (!execution_result.Successful()) {
      SCP_CRITICAL(kPBSInstance, pbs_common::kZeroUuid, execution_result,
                   "Failed to read health port.");
      return execution_result;
    }
  }

  execution_result =
      config_provider->Get(kTotalHttp2ServerThreadsCount,
                           pbs_instance_config.http2server_thread_pool_size);
  if (!execution_result.Successful()) {
    SCP_CRITICAL(kPBSInstance, pbs_common::kZeroUuid, execution_result,
                 "Failed to read http2 server thread pool size.");
    return execution_result;
  }

  pbs_instance_config.http2_server_private_key_file_path =
      std::make_shared<std::string>("");
  pbs_instance_config.http2_server_certificate_file_path =
      std::make_shared<std::string>("");
  // If the "use tls" key exists, then the path to the private key and
  // certificate must be valid, non-empty strings. Otherwise, we just assume the
  // default of false for "use tls"
  if (config_provider
          ->Get(kHttp2ServerUseTls, pbs_instance_config.http2_server_use_tls)
          .Successful() &&
      pbs_instance_config.http2_server_use_tls) {
    if (!config_provider
             ->Get(kHttp2ServerPrivateKeyFilePath,
                   *pbs_instance_config.http2_server_private_key_file_path)
             .Successful() ||
        pbs_instance_config.http2_server_private_key_file_path->empty()) {
      return pbs_common::FailureExecutionResult(
          SC_PBS_INVALID_HTTP2_SERVER_PRIVATE_KEY_FILE_PATH);
    }

    if (!config_provider
             ->Get(kHttp2ServerCertificateFilePath,
                   *pbs_instance_config.http2_server_certificate_file_path)
             .Successful() ||
        pbs_instance_config.http2_server_certificate_file_path->empty()) {
      return pbs_common::FailureExecutionResult(
          SC_PBS_INVALID_HTTP2_SERVER_CERT_FILE_PATH);
    }
  }
  return pbs_instance_config;
}
}  // namespace privacy_sandbox::pbs
