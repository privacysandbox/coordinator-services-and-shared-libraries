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
#include <map>
#include <memory>
#include <string>
#include <vector>
//
// TODO: Delete this file once we have real implementations for all the
// dependencies
//
#include "core/interface/async_executor_interface.h"
#include "core/interface/authorization_service_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/credentials_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "pbs/interface/cloud_platform_dependency_factory_interface.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::pbs {
class DummyMetricClient : public cpio::MetricClientInterface {
 public:
  explicit DummyMetricClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor)
      : async_executor_(async_executor) {}

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult PutMetrics(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>
          record_metric_context) noexcept override {
    // No-op
    return core::FailureExecutionResult(SC_UNKNOWN);
  };

  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
};

class DummyAuthorizationService : public core::AuthorizationServiceInterface {
 public:
  explicit DummyAuthorizationService(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor)
      : async_executor_(async_executor) {}

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Authorize(
      core::AsyncContext<core::AuthorizationRequest,
                         core::AuthorizationResponse>&
          authorization_context) noexcept override {
    authorization_context.response =
        std::make_shared<core::AuthorizationResponse>();
    authorization_context.response->authorized_domain =
        authorization_context.request->claimed_identity;
    authorization_context.result = core::SuccessExecutionResult();
    authorization_context.Finish();
    return core::SuccessExecutionResult();
  }

  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
};

class DummyCredentialsProvider : public core::CredentialsProviderInterface {
 public:
  explicit DummyCredentialsProvider(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor)
      : async_executor_(async_executor) {}

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult GetCredentials(
      core::AsyncContext<core::GetCredentialsRequest,
                         core::GetCredentialsResponse>&
          get_credentials_context) noexcept override {
    get_credentials_context.response =
        std::make_shared<core::GetCredentialsResponse>();
    get_credentials_context.response->access_key_id =
        std::make_shared<std::string>("access_key_id");
    get_credentials_context.response->access_key_secret =
        std::make_shared<std::string>("access_key_secret");
    get_credentials_context.response->security_token =
        std::make_shared<std::string>("security_token");
    get_credentials_context.result = core::SuccessExecutionResult();
    get_credentials_context.Finish();
    return core::SuccessExecutionResult();
  }

  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
};

class DummyInstanceClientProvider
    : public cpio::client_providers::InstanceClientProviderInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  };
};

class DummyPBSClient : public PrivacyBudgetServiceClientInterface {
 public:
  explicit DummyPBSClient(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor)
      : async_executor_(async_executor) {}

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  };

  core::ExecutionResult GetTransactionStatus(
      core::AsyncContext<core::GetTransactionStatusRequest,
                         core::GetTransactionStatusResponse>&
          get_transaction_status_context) noexcept override {
    // Not required for single coordinator testing
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult InitiateConsumeBudgetTransaction(
      core::AsyncContext<ConsumeBudgetTransactionRequest,
                         ConsumeBudgetTransactionResponse>&
          consume_budget_transaction_context) noexcept override {
    // Not required for single coordinator testing
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  core::ExecutionResult ExecuteTransactionPhase(
      core::AsyncContext<core::TransactionPhaseRequest,
                         core::TransactionPhaseResponse>&
          transaction_phase_context) noexcept override {
    // Not required for single coordinator testing
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
};
}  // namespace google::scp::pbs
