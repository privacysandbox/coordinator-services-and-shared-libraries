/*
 * Copyright 2023 Google LLC
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

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/instance_client/instance_client_interface.h"
#include "public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"
#include "public/cpio/utils/configuration_fetcher/interface/configuration_fetcher_interface.h"

#include "error_codes.h"

namespace google::scp::cpio {
/*! @copydoc ConfigurationFetcherInterface
 */
class ConfigurationFetcher : public ConfigurationFetcherInterface {
 public:
  explicit ConfigurationFetcher(InstanceClientInterface* instance_client,
                                ParameterClientInterface* parameter_client)
      : instance_client_(instance_client),
        parameter_client_(parameter_client) {}

  core::ExecutionResultOr<std::string> GetParameterByName(
      std::string parameter_name) noexcept override;

  core::ExecutionResult GetParameterByNameAsync(
      core::AsyncContext<std::string, std::string> context) noexcept override;

  core::ExecutionResultOr<LogOption> GetSharedLogOption(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetSharedLogOptionAsync(
      core::AsyncContext<GetConfigurationRequest, LogOption> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetSharedCpuThreadCount(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetSharedCpuThreadCountAsync(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetSharedCpuThreadPoolQueueCap(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetSharedCpuThreadPoolQueueCapAsync(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetSharedIoThreadCount(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetSharedIoThreadCountAsync(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<size_t> GetSharedIoThreadPoolQueueCap(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetSharedIoThreadPoolQueueCapAsync(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetJobClientJobQueueName(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetJobClientJobQueueNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetJobClientJobTableName(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetJobClientJobTableNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetGcpJobClientSpannerInstanceName(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetGcpJobClientSpannerInstanceNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string> GetGcpJobClientSpannerDatabaseName(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetGcpJobClientSpannerDatabaseNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string>
  GetGcpNosqlDatabaseClientSpannerInstanceName(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetGcpNosqlDatabaseClientSpannerInstanceNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<std::string>
  GetGcpNosqlDatabaseClientSpannerDatabaseName(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetGcpNosqlDatabaseClientSpannerDatabaseNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;
  core::ExecutionResultOr<std::string> GetQueueClientQueueName(
      GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetQueueClientQueueNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string> context) noexcept
      override;

  core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeKem>
  GetCryptoClientHpkeKem(GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetCryptoClientHpkeKemAsync(
      core::AsyncContext<GetConfigurationRequest,
                         cmrt::sdk::crypto_service::v1::HpkeKem>
          context) noexcept override;

  core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeKdf>
  GetCryptoClientHpkeKdf(GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetCryptoClientHpkeKdfAsync(
      core::AsyncContext<GetConfigurationRequest,
                         cmrt::sdk::crypto_service::v1::HpkeKdf>
          context) noexcept override;

  core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeAead>
  GetCryptoClientHpkeAead(GetConfigurationRequest request) noexcept override;

  core::ExecutionResult GetCryptoClientHpkeAeadAsync(
      core::AsyncContext<GetConfigurationRequest,
                         cmrt::sdk::crypto_service::v1::HpkeAead>
          context) noexcept override;

 private:
  core::AsyncContext<std::string, std::string> ContextConvertCallback(
      const std::string& parameter_name,
      core::AsyncContext<GetConfigurationRequest, std::string>&
          context_without_parameter_name) noexcept;

  core::ExecutionResult GetConfiguration(
      core::AsyncContext<std::string, std::string>&
          get_configuration_context) noexcept;

  void GetCurrentInstanceResourceNameCallback(
      const core::ExecutionResult& result,
      cmrt::sdk::instance_service::v1::GetCurrentInstanceResourceNameResponse
          response,
      core::AsyncContext<std::string, std::string>&
          get_configuration_context) noexcept;

  void GetInstanceDetailsByResourceNameCallback(
      const core::ExecutionResult& result,
      cmrt::sdk::instance_service::v1::GetInstanceDetailsByResourceNameResponse
          get_instance_details_response,
      const cmrt::sdk::instance_service::v1::
          GetCurrentInstanceResourceNameResponse& get_current_instance_response,
      core::AsyncContext<std::string, std::string>&
          get_configuration_context) noexcept;

  void GetParameterCallback(
      const core::ExecutionResult& result,
      cmrt::sdk::parameter_service::v1::GetParameterResponse response,
      core::AsyncContext<std::string, std::string>&
          get_configuration_context) noexcept;

  InstanceClientInterface* instance_client_;
  ParameterClientInterface* parameter_client_;
};
}  // namespace google::scp::cpio
