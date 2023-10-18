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

#include <string>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

namespace google::scp::cpio {
/// Request to get configuration.
struct GetConfigurationRequest {
  virtual ~GetConfigurationRequest() = default;
};

/**
 * @brief Helper to fetch configurations.
 */
class ConfigurationFetcherInterface {
 public:
  virtual ~ConfigurationFetcherInterface() = default;

  /**
   * @brief Get parameter by name.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the SharedLogOption and
   * result.
   */
  virtual core::ExecutionResultOr<std::string> GetParameterByName(
      std::string parameter_name) noexcept = 0;

  /**
   * @brief Get parameter by name.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetParameterByNameAsync(
      core::AsyncContext<std::string, std::string> context) noexcept = 0;

  /**** Shared configurations start */
  /**
   * @brief Get SharedLogOption.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the SharedLogOption and
   * result.
   */
  virtual core::ExecutionResultOr<LogOption> GetSharedLogOption(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get SharedLogOption.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetSharedLogOptionAsync(
      core::AsyncContext<GetConfigurationRequest, LogOption>
          context) noexcept = 0;

  /**
   * @brief Get the CPU thread count for the shared CPU thread pool.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the CPU thread count and
   * result.
   */
  virtual core::ExecutionResultOr<size_t> GetSharedCpuThreadCount(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the CPU thread count for the shared CPU thread pool.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetSharedCpuThreadCountAsync(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the queue cap for the shared CPU thread pool.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the queue cap and
   * result.
   */
  virtual core::ExecutionResultOr<size_t> GetSharedCpuThreadPoolQueueCap(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the queue cap for the shared CPU thread pool.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetSharedCpuThreadPoolQueueCapAsync(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the IO thread count for the shared IO thread pool.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the IO thread count and
   * result.
   */
  virtual core::ExecutionResultOr<size_t> GetSharedIoThreadCount(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the IO thread count for the shared IO thread pool.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetSharedIoThreadCountAsync(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**
   * @brief Get the queue cap for the shared Io thread pool.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the queue cap and
   * result.
   */
  virtual core::ExecutionResultOr<size_t> GetSharedIoThreadPoolQueueCap(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the queue cap for the shared IO thread pool.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetSharedIoThreadPoolQueueCapAsync(
      core::AsyncContext<GetConfigurationRequest, size_t> context) noexcept = 0;

  /**** Shared configurations end */

  /**** JobClient configurations start */
  /**
   * @brief Get the Job Client Job Queue Name value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the Job Queue
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string> GetJobClientJobQueueName(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client Job Queue Name value
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetJobClientJobQueueNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get the Job Client Job Table Name value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the Job Table
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string> GetJobClientJobTableName(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client Job Table Name value
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetJobClientJobTableNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get the Job Client GCP Job Spanner Instance Name value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the GCP Job Spanner Instance
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetGcpJobClientSpannerInstanceName(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client GCP Job Spanner Instance Name value
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetGcpJobClientSpannerInstanceNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**
   * @brief Get the Job Client GCP Job Spanner Database Name value.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the GCP Job Spanner Database
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetGcpJobClientSpannerDatabaseName(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the Job Client GCP Job Spanner Database Name value
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetGcpJobClientSpannerDatabaseNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**** JobClient configurations end */

  /**** NosqlDatabaseClient configurations start */
  /**
   * @brief Get the GcpNosqlDatabaseClientSpannerInstanceName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the name and result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetGcpNosqlDatabaseClientSpannerInstanceName(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the GcpNosqlDatabaseClientSpannerInstanceName.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult
  GetGcpNosqlDatabaseClientSpannerInstanceNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;
  /**
   * @brief Get the GcpNosqlDatabaseClientSpannerDatabaseName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the name and result.
   */
  virtual core::ExecutionResultOr<std::string>
  GetGcpNosqlDatabaseClientSpannerDatabaseName(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get the GcpNosqlDatabaseClientSpannerDatabaseName.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult
  GetGcpNosqlDatabaseClientSpannerDatabaseNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;

  /**** NosqlDatabaseClient configurations end */

  /**** QueueClient configurations start */
  /**
   * @brief Get QueueClientQueueName.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the Job Queue
   * Name and result.
   */
  virtual core::ExecutionResultOr<std::string> GetQueueClientQueueName(
      GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get QueueClientQueueName.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetQueueClientQueueNameAsync(
      core::AsyncContext<GetConfigurationRequest, std::string>
          context) noexcept = 0;
  /**** QueueClient configurations end */

  /**** CryptoClient configurations start */
  /**
   * @brief Get CryptoClientHpkeKem.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the CryptoClientHpkeKem and
   * result.
   */
  virtual core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeKem>
  GetCryptoClientHpkeKem(GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get CryptoClientHpkeKem.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetCryptoClientHpkeKemAsync(
      core::AsyncContext<GetConfigurationRequest,
                         cmrt::sdk::crypto_service::v1::HpkeKem>
          context) noexcept = 0;

  /**
   * @brief Get CryptoClientHpkeKdf.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the CryptoClientHpkeKdf and
   * result.
   */
  virtual core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeKdf>
  GetCryptoClientHpkeKdf(GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get CryptoClientHpkeKdf.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetCryptoClientHpkeKdfAsync(
      core::AsyncContext<GetConfigurationRequest,
                         cmrt::sdk::crypto_service::v1::HpkeKdf>
          context) noexcept = 0;

  /**
   * @brief Get CryptoClientHpkeAead.
   *
   * @param request get configuration request.
   * @return core::ExecutionResultOr<std::string> the CryptoClientHpkeAead and
   * result.
   */
  virtual core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeAead>
  GetCryptoClientHpkeAead(GetConfigurationRequest request) noexcept = 0;

  /**
   * @brief Get CryptoClientHpkeAead.
   *
   * @param context the async context for the operation.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult GetCryptoClientHpkeAeadAsync(
      core::AsyncContext<GetConfigurationRequest,
                         cmrt::sdk::crypto_service::v1::HpkeAead>
          context) noexcept = 0;
  /**** CryptoClient configurations end */
};

}  // namespace google::scp::cpio
