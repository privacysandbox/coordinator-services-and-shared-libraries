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

#include "configuration_fetcher.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/server/interface/configuration_keys.h"
#include "cpio/server/interface/crypto_service/configuration_keys.h"
#include "cpio/server/interface/job_service/configuration_keys.h"
#include "cpio/server/interface/nosql_database_service/configuration_keys.h"
#include "cpio/server/interface/queue_service/configuration_keys.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "public/cpio/utils/configuration_fetcher/interface/configuration_keys.h"
#include "public/cpio/utils/sync_utils/sync_utils.h"

#include "configuration_fetcher_utils.h"
#include "error_codes.h"

using google::cmrt::sdk::crypto_service::v1::HpkeAead;
using google::cmrt::sdk::crypto_service::v1::HpkeKdf;
using google::cmrt::sdk::crypto_service::v1::HpkeKem;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::placeholders::_1;
using std::placeholders::_2;

namespace {
constexpr char kConfigurationFetcher[] = "ConfigurationFetcher";
constexpr char kEnvNameTag[] = "environment-name";
}  // namespace

namespace google::scp::cpio {
ExecutionResultOr<string> ConfigurationFetcher::GetParameterByName(
    string parameter_name) noexcept {
  string parameter;
  auto execution_result = SyncUtils::AsyncToSync<string, string>(
      bind(&ConfigurationFetcher::GetParameterByNameAsync, this, _1),
      parameter_name, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetParameterByName for %s.",
                            parameter_name.c_str());
  return parameter;
}

ExecutionResult ConfigurationFetcher::GetParameterByNameAsync(
    AsyncContext<string, string> context) noexcept {
  return GetConfiguration(context);
}

ExecutionResultOr<LogOption> ConfigurationFetcher::GetSharedLogOption(
    GetConfigurationRequest request) noexcept {
  LogOption parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, LogOption>(
          bind(&ConfigurationFetcher::GetSharedLogOptionAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetSharedLogOption %s.",
                            kSdkClientLogOption);
  return parameter;
}

core::ExecutionResult ConfigurationFetcher::GetSharedLogOptionAsync(
    AsyncContext<GetConfigurationRequest, LogOption> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<LogOption>(
          kSdkClientLogOption, context,
          bind(ConfigurationFetcherUtils::StringToEnum<LogOption>, _1,
               kLogOptionConfigMap));
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t> ConfigurationFetcher::GetSharedCpuThreadCount(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetSharedCpuThreadCountAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetSharedCpuThreadCount %s.",
                            kSharedCpuThreadCount);
  return parameter;
}

ExecutionResult ConfigurationFetcher::GetSharedCpuThreadCountAsync(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          kSharedCpuThreadCount, context,
          bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t> ConfigurationFetcher::GetSharedCpuThreadPoolQueueCap(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetSharedCpuThreadPoolQueueCapAsync, this,
               _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetSharedCpuThreadPoolQueueCap %s.",
                            kSharedCpuThreadCount);
  return parameter;
}

ExecutionResult ConfigurationFetcher::GetSharedCpuThreadPoolQueueCapAsync(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          kSharedCpuThreadPoolQueueCap, context,
          bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t> ConfigurationFetcher::GetSharedIoThreadCount(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetSharedIoThreadCountAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetSharedIoThreadCount %s.",
                            kSharedIoThreadCount);
  return parameter;
}

ExecutionResult ConfigurationFetcher::GetSharedIoThreadCountAsync(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          kSharedIoThreadCount, context,
          bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<size_t> ConfigurationFetcher::GetSharedIoThreadPoolQueueCap(
    GetConfigurationRequest request) noexcept {
  size_t parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, size_t>(
          bind(&ConfigurationFetcher::GetSharedIoThreadPoolQueueCapAsync, this,
               _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetSharedIoThreadPoolQueueCap %s.",
                            kSharedIoThreadCount);
  return parameter;
}

ExecutionResult ConfigurationFetcher::GetSharedIoThreadPoolQueueCapAsync(
    AsyncContext<GetConfigurationRequest, size_t> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<size_t>(
          kSharedIoThreadPoolQueueCap, context,
          bind(ConfigurationFetcherUtils::StringToUInt<size_t>, _1));
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string> ConfigurationFetcher::GetJobClientJobQueueName(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetJobClientJobQueueNameAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetJobClientJobQueueName %s.",
                            kJobClientJobQueueName);
  return parameter;
}

core::ExecutionResult ConfigurationFetcher::GetJobClientJobQueueNameAsync(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(kJobClientJobQueueName, context);
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string> ConfigurationFetcher::GetJobClientJobTableName(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetJobClientJobTableNameAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetJobClientJobTableName %s.",
                            kJobClientJobTableName);
  return parameter;
}

core::ExecutionResult ConfigurationFetcher::GetJobClientJobTableNameAsync(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(kJobClientJobTableName, context);
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetGcpJobClientSpannerInstanceName(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetGcpJobClientSpannerInstanceNameAsync,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,

                            "Failed to GetGcpJobClientSpannerInstanceName %s.",
                            kGcpJobClientSpannerInstanceName);
  return parameter;
}

core::ExecutionResult
ConfigurationFetcher::GetGcpJobClientSpannerInstanceNameAsync(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(kGcpJobClientSpannerInstanceName, context);
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetGcpJobClientSpannerDatabaseName(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetGcpJobClientSpannerDatabaseNameAsync,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,

                            "Failed to GetGcpJobClientSpannerDatabaseName %s.",
                            kGcpJobClientSpannerDatabaseName);
  return parameter;
}

core::ExecutionResult
ConfigurationFetcher::GetGcpJobClientSpannerDatabaseNameAsync(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(kGcpJobClientSpannerDatabaseName, context);
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetGcpNosqlDatabaseClientSpannerInstanceName(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::
                   GetGcpNosqlDatabaseClientSpannerInstanceNameAsync,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetGcpNosqlDatabaseClientSpannerInstanceName %s.",
      kGcpNoSQLDatabaseClientSpannerInstanceName);
  return parameter;
}

core::ExecutionResult
ConfigurationFetcher::GetGcpNosqlDatabaseClientSpannerInstanceNameAsync(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      kGcpNoSQLDatabaseClientSpannerInstanceName, context);
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string>
ConfigurationFetcher::GetGcpNosqlDatabaseClientSpannerDatabaseName(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::
                   GetGcpNosqlDatabaseClientSpannerDatabaseNameAsync,
               this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(
      execution_result, kConfigurationFetcher, kZeroUuid,
      "Failed to GetGcpNosqlDatabaseClientSpannerDatabaseName %s.",
      kGcpNoSQLDatabaseClientSpannerDatabaseName);
  return parameter;
}

core::ExecutionResult
ConfigurationFetcher::GetGcpNosqlDatabaseClientSpannerDatabaseNameAsync(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name = ContextConvertCallback(
      kGcpNoSQLDatabaseClientSpannerDatabaseName, context);
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<string> ConfigurationFetcher::GetQueueClientQueueName(
    GetConfigurationRequest request) noexcept {
  string parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, string>(
          bind(&ConfigurationFetcher::GetQueueClientQueueNameAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetQueueClientQueueName %s.",
                            kQueueClientQueueName);
  return parameter;
}

core::ExecutionResult ConfigurationFetcher::GetQueueClientQueueNameAsync(
    AsyncContext<GetConfigurationRequest, string> context) noexcept {
  auto context_with_parameter_name =
      ContextConvertCallback(kQueueClientQueueName, context);
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<HpkeKem> ConfigurationFetcher::GetCryptoClientHpkeKem(
    GetConfigurationRequest request) noexcept {
  HpkeKem parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, HpkeKem>(
          bind(&ConfigurationFetcher::GetCryptoClientHpkeKemAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetCryptoClientHpkeKem %s.",
                            kCryptoClientHpkeKem);
  return parameter;
}

core::ExecutionResult ConfigurationFetcher::GetCryptoClientHpkeKemAsync(
    AsyncContext<GetConfigurationRequest, HpkeKem> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<HpkeKem>(
          kCryptoClientHpkeKem, context,
          bind(ConfigurationFetcherUtils::StringToEnum<HpkeKem>, _1,
               kHpkeKemConfigMap));
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<HpkeKdf> ConfigurationFetcher::GetCryptoClientHpkeKdf(
    GetConfigurationRequest request) noexcept {
  HpkeKdf parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, HpkeKdf>(
          bind(&ConfigurationFetcher::GetCryptoClientHpkeKdfAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetCryptoClientHpkeKdf %s.",
                            kCryptoClientHpkeKdf);
  return parameter;
}

core::ExecutionResult ConfigurationFetcher::GetCryptoClientHpkeKdfAsync(
    AsyncContext<GetConfigurationRequest, HpkeKdf> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<HpkeKdf>(
          kCryptoClientHpkeKdf, context,
          bind(ConfigurationFetcherUtils::StringToEnum<HpkeKdf>, _1,
               kHpkeKdfConfigMap));
  return GetConfiguration(context_with_parameter_name);
}

ExecutionResultOr<HpkeAead> ConfigurationFetcher::GetCryptoClientHpkeAead(
    GetConfigurationRequest request) noexcept {
  HpkeAead parameter;
  auto execution_result =
      SyncUtils::AsyncToSync<GetConfigurationRequest, HpkeAead>(
          bind(&ConfigurationFetcher::GetCryptoClientHpkeAeadAsync, this, _1),
          request, parameter);
  RETURN_AND_LOG_IF_FAILURE(execution_result, kConfigurationFetcher, kZeroUuid,
                            "Failed to GetCryptoClientHpkeAead %s.",
                            kCryptoClientHpkeAead);
  return parameter;
}

core::ExecutionResult ConfigurationFetcher::GetCryptoClientHpkeAeadAsync(
    AsyncContext<GetConfigurationRequest, HpkeAead> context) noexcept {
  auto context_with_parameter_name =
      ConfigurationFetcherUtils::ContextConvertCallback<HpkeAead>(
          kCryptoClientHpkeAead, context,
          bind(ConfigurationFetcherUtils::StringToEnum<HpkeAead>, _1,
               kHpkeAeadConfigMap));
  return GetConfiguration(context_with_parameter_name);
}

AsyncContext<string, string> ConfigurationFetcher::ContextConvertCallback(
    const string& parameter_name, AsyncContext<GetConfigurationRequest, string>&
                                      context_without_parameter_name) noexcept {
  return ConfigurationFetcherUtils::ContextConvertCallback<string>(
      parameter_name, context_without_parameter_name,
      [](const string& value) { return value; });
}

core::ExecutionResult ConfigurationFetcher::GetConfiguration(
    AsyncContext<string, string>& get_configuration_context) noexcept {
  return instance_client_->GetCurrentInstanceResourceName(
      GetCurrentInstanceResourceNameRequest(),
      bind(&ConfigurationFetcher::GetCurrentInstanceResourceNameCallback, this,
           _1, _2, get_configuration_context));
}

void ConfigurationFetcher::GetCurrentInstanceResourceNameCallback(
    const ExecutionResult& result,
    GetCurrentInstanceResourceNameResponse response,
    AsyncContext<string, string>& get_configuration_context) noexcept {
  if (!result.Successful()) {
    get_configuration_context.result = result;
    SCP_ERROR_CONTEXT(kConfigurationFetcher, get_configuration_context, result,
                      "Failed to GetCurrentInstanceResourceName");
    get_configuration_context.Finish();
    return;
  }

  GetInstanceDetailsByResourceNameRequest request;
  request.set_instance_resource_name(response.instance_resource_name());
  if (auto result = instance_client_->GetInstanceDetailsByResourceName(
          move(request),
          bind(&ConfigurationFetcher::GetInstanceDetailsByResourceNameCallback,
               this, _1, _2, response, get_configuration_context));
      !result.Successful()) {
    get_configuration_context.result = result;
    SCP_ERROR_CONTEXT(
        kConfigurationFetcher, get_configuration_context, result,
        "Failed to GetInstanceDetailsByResourceName for instance %s",
        response.instance_resource_name().c_str());
    get_configuration_context.Finish();
  }
}

void ConfigurationFetcher::GetInstanceDetailsByResourceNameCallback(
    const ExecutionResult& result,
    GetInstanceDetailsByResourceNameResponse get_instance_details_response,
    const GetCurrentInstanceResourceNameResponse& get_current_instance_response,
    AsyncContext<string, string>& get_configuration_context) noexcept {
  if (!result.Successful()) {
    get_configuration_context.result = result;
    SCP_ERROR_CONTEXT(
        kConfigurationFetcher, get_configuration_context, result,
        "Failed to GetInstanceDetailsByResourceName for instance %s",
        get_current_instance_response.instance_resource_name().c_str());
    get_configuration_context.Finish();
    return;
  }

  auto it = get_instance_details_response.instance_details().labels().find(
      string(kEnvNameTag));
  if (it == get_instance_details_response.instance_details().labels().end()) {
    get_configuration_context.result = FailureExecutionResult(
        SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kConfigurationFetcher, get_configuration_context,
        get_configuration_context.result,
        "Failed to find environment name for instance %s",
        get_current_instance_response.instance_resource_name().c_str());
    get_configuration_context.Finish();
    return;
  }

  GetParameterRequest request;
  request.set_parameter_name(absl::StrCat("scp-", it->second, "-",
                                          *get_configuration_context.request));
  if (auto result = parameter_client_->GetParameter(
          move(request), bind(&ConfigurationFetcher::GetParameterCallback, this,
                              _1, _2, get_configuration_context));
      !result.Successful()) {
    get_configuration_context.result = result;
    SCP_ERROR_CONTEXT(kConfigurationFetcher, get_configuration_context,
                      get_configuration_context.result,
                      "Failed to get parameter value for %s",
                      get_configuration_context.request->c_str());
    get_configuration_context.Finish();
  }
}

void ConfigurationFetcher::GetParameterCallback(
    const ExecutionResult& result, GetParameterResponse response,
    AsyncContext<string, string>& get_configuration_context) noexcept {
  if (!result.Successful()) {
    get_configuration_context.result = result;
    SCP_ERROR_CONTEXT(kConfigurationFetcher, get_configuration_context,
                      get_configuration_context.result,
                      "Failed to get parameter value for %s",
                      get_configuration_context.request->c_str());
    get_configuration_context.Finish();
    return;
  }

  get_configuration_context.result = SuccessExecutionResult();
  get_configuration_context.response =
      make_shared<string>(move(response.parameter_value()));
  get_configuration_context.Finish();
}
}  // namespace google::scp::cpio
