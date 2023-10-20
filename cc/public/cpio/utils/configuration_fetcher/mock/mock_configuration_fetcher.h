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

#include <gmock/gmock.h>

#include <string>

#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"
#include "public/cpio/utils/configuration_fetcher/interface/configuration_fetcher_interface.h"

namespace google::scp::cpio {
class MockConfigurationFetcher : public ConfigurationFetcherInterface {
 public:
  MockConfigurationFetcher() {}

  MOCK_METHOD(core::ExecutionResultOr<std::string>, GetParameterByName,
              ((std::string)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetParameterByNameAsync,
              ((core::AsyncContext<std::string, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<LogOption>, GetSharedLogOption,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetSharedLogOptionAsync,
              ((core::AsyncContext<GetConfigurationRequest, LogOption>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>, GetSharedCpuThreadCount,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetSharedCpuThreadCountAsync,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>, GetSharedCpuThreadPoolQueueCap,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetSharedCpuThreadPoolQueueCapAsync,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>, GetSharedIoThreadCount,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetSharedIoThreadCountAsync,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<size_t>, GetSharedIoThreadPoolQueueCap,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetSharedIoThreadPoolQueueCapAsync,
              ((core::AsyncContext<GetConfigurationRequest, size_t>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>, GetJobClientJobQueueName,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetJobClientJobQueueNameAsync,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>, GetJobClientJobTableName,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetJobClientJobTableNameAsync,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetGcpJobClientSpannerInstanceName, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetGcpJobClientSpannerInstanceNameAsync,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetGcpJobClientSpannerDatabaseName, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetGcpJobClientSpannerDatabaseNameAsync,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetGcpNosqlDatabaseClientSpannerInstanceName,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult,
              GetGcpNosqlDatabaseClientSpannerInstanceNameAsync,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>,
              GetGcpNosqlDatabaseClientSpannerDatabaseName,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult,
              GetGcpNosqlDatabaseClientSpannerDatabaseNameAsync,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<std::string>, GetQueueClientQueueName,
              ((GetConfigurationRequest)), (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetQueueClientQueueNameAsync,
              ((core::AsyncContext<GetConfigurationRequest, std::string>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeKem>,
              GetCryptoClientHpkeKem, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetCryptoClientHpkeKemAsync,
              ((core::AsyncContext<GetConfigurationRequest,
                                   cmrt::sdk::crypto_service::v1::HpkeKem>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeKdf>,
              GetCryptoClientHpkeKdf, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetCryptoClientHpkeKdfAsync,
              ((core::AsyncContext<GetConfigurationRequest,
                                   cmrt::sdk::crypto_service::v1::HpkeKdf>)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResultOr<cmrt::sdk::crypto_service::v1::HpkeAead>,
              GetCryptoClientHpkeAead, ((GetConfigurationRequest)),
              (noexcept, override));

  MOCK_METHOD(core::ExecutionResult, GetCryptoClientHpkeAeadAsync,
              ((core::AsyncContext<GetConfigurationRequest,
                                   cmrt::sdk::crypto_service::v1::HpkeAead>)),
              (noexcept, override));
};
}  // namespace google::scp::cpio
