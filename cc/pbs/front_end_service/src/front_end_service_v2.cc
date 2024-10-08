// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cc/pbs/front_end_service/src/front_end_service_v2.h"

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/core/interface/errors.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/logger_interface.h"
#include "cc/core/interface/type_def.h"
#include "cc/pbs/consume_budget/src/gcp/error_codes.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/front_end_service/src/front_end_utils.h"
#include "cc/pbs/front_end_service/src/metric_initialization.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/interface/consume_budget_interface.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/metric_client/metric_client_interface.h"
#include "cc/public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "cc/public/cpio/utils/metric_aggregation/interface/type_def.h"

namespace google::scp::pbs {
namespace {

using ::google::scp::core::AsyncContext;
using ::google::scp::core::AsyncExecutorInterface;
using ::google::scp::core::ExecutionResult;
using ::google::scp::core::ExecutionResultOr;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::HttpHandler;
using ::google::scp::core::HttpHeaders;
using ::google::scp::core::HttpMethod;
using ::google::scp::core::HttpRequest;
using ::google::scp::core::HttpResponse;
using ::google::scp::core::HttpServerInterface;
using ::google::scp::core::kAggregatedMetricIntervalMs;
using ::google::scp::core::kDefaultAggregatedMetricIntervalMs;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::Timestamp;
using ::google::scp::core::common::kZeroUuid;
using ::google::scp::core::common::ToString;
using ::google::scp::core::common::Uuid;
using ::google::scp::core::errors::
    SC_PBS_FRONT_END_SERVICE_GET_TRANSACTION_STATUS_RETURNS_404_BY_DEFAULT;
using ::google::scp::core::errors::
    SC_PBS_FRONT_END_SERVICE_INITIALIZATION_FAILED;
using ::google::scp::core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST;
using ::google::scp::core::errors::
    SC_PBS_FRONT_END_SERVICE_UNABLE_TO_FIND_TRANSACTION_METRICS;
using ::google::scp::cpio::AggregateMetricInterface;
using ::google::scp::cpio::MetricLabels;
using ::google::scp::cpio::MetricName;
using ::google::scp::pbs::FrontEndUtils;

inline constexpr char kFrontEndService[] = "FrontEndServiceV2";
inline constexpr char kFakeLastExecutionTimestamp[] = "1234";

ExecutionResultOr<std::shared_ptr<AggregateMetricInterface>>
FindAggregateMetricInMap(const MetricsMap& metrics_map,
                         absl::string_view metric_label,
                         absl::string_view metric_name) {
  auto outer_map_iterator = metrics_map.find(metric_label);
  if (outer_map_iterator == metrics_map.end()) {
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_UNABLE_TO_FIND_TRANSACTION_METRICS);
  }
  auto inner_map_iterator = outer_map_iterator->second.find(metric_name);
  if (inner_map_iterator == outer_map_iterator->second.end()) {
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_UNABLE_TO_FIND_TRANSACTION_METRICS);
  }
  return inner_map_iterator->second;
}

// The extracted transaction_id is unused in BeginTransaction of
// front_end_service_v2.cc, but the extraction serves two purposes:
// 1. Make sure that the client is continues to adhere to the client server
//    interaction contract that was previously enforced by
//    front_end_service.cc
// 2. Make sure that the transaction ID in the header can be extracted without
//    any error. If the transaction ID can be extracted in BeginTransaction,
//    it is likely that it can also be extracted in PrepareTransaction, so it
//    helps PBS detect potential extraction issue earlier.
// The same reasoning applies to transaction_secret in BeginTransaction and
// the extraction of transaction_id and transaction_secret in other phases.
//
// The transaction_id will be returned if there isn't any extraction issue. The
// transaction_id will only be used for logging purposes.
ExecutionResultOr<std::string> ExtractBackwardCompatibleHeaders(
    const AsyncContext<HttpRequest, HttpResponse>& http_context,
    bool should_extract_last_execution_timestamp) {
  if (http_context.request == nullptr ||
      http_context.request->headers == nullptr) {
    return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
  }

  Uuid transaction_id;
  if (auto execution_result = FrontEndUtils::ExtractTransactionId(
          http_context.request->headers, transaction_id);
      !execution_result.Successful()) {
    return execution_result;
  }

  std::string transaction_secret;
  if (auto execution_result = FrontEndUtils::ExtractTransactionSecret(
          http_context.request->headers, transaction_secret);
      !execution_result.Successful()) {
    return execution_result;
  }

  if (should_extract_last_execution_timestamp) {
    Timestamp last_execution_timestamp;
    if (auto execution_result = FrontEndUtils::ExtractLastExecutionTimestamp(
            http_context.request->headers, last_execution_timestamp);
        !execution_result.Successful()) {
      return execution_result;
    }
  }
  return ToString(transaction_id);
}

// The last execution timestamp was used in front_end_service.cc to support
// optimistic concurrency control in two-phase commit transaction.
// front_end_service_v2.cc does not support such concurrency control, but it
// is still returning this header to the client for backward compatibility
// purpose.
void InsertBackwardCompatibleHeaders(
    AsyncContext<HttpRequest, HttpResponse>& http_context) {
  http_context.response->headers = std::make_shared<HttpHeaders>();
  http_context.response->headers->insert(
      {kTransactionLastExecutionTimestampHeader, kFakeLastExecutionTimestamp});
}
}  // namespace

FrontEndServiceV2::FrontEndServiceV2(
    std::shared_ptr<core::HttpServerInterface> http_server,
    std::shared_ptr<core::AsyncExecutorInterface> async_executor,
    const std::shared_ptr<cpio::MetricClientInterface> metric_client,
    const std::shared_ptr<core::ConfigProviderInterface> config_provider,
    BudgetConsumptionHelperInterface* budget_consumption_helper,
    std::unique_ptr<MetricInitialization> metric_initialization)
    : http_server_(http_server),
      async_executor_(async_executor),
      metric_client_(metric_client),
      config_provider_(config_provider),
      aggregated_metric_interval_ms_(kDefaultAggregatedMetricIntervalMs),
      metric_initialization_(std::move(metric_initialization)),
      budget_consumption_helper_(budget_consumption_helper) {
  meter_ = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
      "Frontend Service v2", "2.0");
  total_request_counter_ = meter_->CreateUInt64Counter(
      kMetricNameRequests, "Total number of requests received");
  client_error_counter_ = meter_->CreateUInt64Counter(
      kMetricNameClientErrors, "Number of client errors (4xx status codes)");
  server_error_counter_ = meter_->CreateUInt64Counter(
      kMetricNameServerErrors, "Number of server errors (5xx status codes)");
}

ExecutionResult FrontEndServiceV2::Init() noexcept {
  ExecutionResult execution_result =
      config_provider_->Get(kRemotePrivacyBudgetServiceClaimedIdentity,
                            remote_coordinator_claimed_identity_);

  RETURN_IF_FAILURE(execution_result);

  std::string health_check_path(kStatusHealthCheckPath);
  HttpHandler health_check_handler =
      absl::bind_front(&FrontEndServiceV2::BeginTransaction, this);
  http_server_->RegisterResourceHandler(HttpMethod::POST, health_check_path,
                                        health_check_handler);

  std::string consume_budget_path(kStatusConsumeBudgetPath);
  HttpHandler consume_budget_handler =
      absl::bind_front(&FrontEndServiceV2::PrepareTransaction, this);
  http_server_->RegisterResourceHandler(HttpMethod::POST, consume_budget_path,
                                        consume_budget_handler);

  std::string begin_transaction_path(kBeginTransactionPath);
  HttpHandler begin_transaction_handler =
      absl::bind_front(&FrontEndServiceV2::BeginTransaction, this);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, begin_transaction_path, begin_transaction_handler);

  std::string prepare_transaction_path(kPrepareTransactionPath);
  HttpHandler prepare_transaction_handler =
      absl::bind_front(&FrontEndServiceV2::PrepareTransaction, this);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, prepare_transaction_path, prepare_transaction_handler);

  std::string commit_transaction_path(kCommitTransactionPath);
  HttpHandler commit_transaction_handler =
      absl::bind_front(&FrontEndServiceV2::CommitTransaction, this);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, commit_transaction_path, commit_transaction_handler);

  std::string notify_transaction_path(kNotifyTransactionPath);
  HttpHandler notify_transaction_handler =
      absl::bind_front(&FrontEndServiceV2::NotifyTransaction, this);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, notify_transaction_path, notify_transaction_handler);

  std::string abort_transaction_path(kAbortTransactionPath);
  HttpHandler abort_transaction_handler =
      absl::bind_front(&FrontEndServiceV2::AbortTransaction, this);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, abort_transaction_path, abort_transaction_handler);

  std::string end_transaction_path(kEndTransactionPath);
  HttpHandler end_transaction_handler =
      absl::bind_front(&FrontEndServiceV2::EndTransaction, this);
  http_server_->RegisterResourceHandler(HttpMethod::POST, end_transaction_path,
                                        end_transaction_handler);

  std::string get_transaction_status_path(kStatusTransactionPath);
  HttpHandler get_transaction_transaction_handler =
      absl::bind_front(&FrontEndServiceV2::GetTransactionStatus, this);
  http_server_->RegisterResourceHandler(HttpMethod::GET,
                                        get_transaction_status_path,
                                        get_transaction_transaction_handler);

  if (!config_provider_
           ->Get(kAggregatedMetricIntervalMs, aggregated_metric_interval_ms_)
           .Successful()) {
    aggregated_metric_interval_ms_ = kDefaultAggregatedMetricIntervalMs;
  }

  if (budget_consumption_helper_ == nullptr) {
    auto failure_execution_result =
        FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INITIALIZATION_FAILED);
    SCP_ERROR(kFrontEndService, kZeroUuid, failure_execution_result,
              "BudgetConsumptionHelper is nullptr during initialization of "
              "FrontEndServiceV2.");
    return failure_execution_result;
  }

  if (metric_initialization_ == nullptr) {
    metric_initialization_ =
        std::make_unique<MetricInitializationImplementation>();
  }
  // Initializes TransactionMetric instances for all transaction phases.
  ASSIGN_OR_RETURN(metrics_instances_map_, metric_initialization_->Initialize(
                                               async_executor_, metric_client_,
                                               aggregated_metric_interval_ms_))

  for (auto const& [method_name, map] : metrics_instances_map_) {
    for (auto const& [metric_name, metric_instance] : map) {
      RETURN_IF_FAILURE(metric_instance->Init());
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult FrontEndServiceV2::Run() noexcept {
  // Runs all AggregateMetric instances.
  for (auto const& [method_name, map] : metrics_instances_map_) {
    for (auto const& [metric_name, metric_instance] : map) {
      RETURN_IF_FAILURE(metric_instance->Run());
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult FrontEndServiceV2::Stop() noexcept {
  // Shuts down all AggregateMetric instances.
  for (auto const& [method_name, map] : metrics_instances_map_) {
    for (auto const& [metric_name, metric_instance] : map) {
      RETURN_IF_FAILURE(metric_instance->Stop());
    }
  }
  return SuccessExecutionResult();
}

std::shared_ptr<std::string> FrontEndServiceV2::ObtainTransactionOrigin(
    AsyncContext<HttpRequest, HttpResponse>& http_context) const {
  // If transaction origin is supplied in the header use that instead. The
  // transaction origin in the header is useful if a peer coordinator is
  // resolving a transaction on behalf of a client.
  std::string transaction_origin_in_header;
  auto execution_result = FrontEndUtils::ExtractTransactionOrigin(
      http_context.request->headers, transaction_origin_in_header);
  if (execution_result.Successful() && !transaction_origin_in_header.empty()) {
    return std::make_shared<std::string>(
        std::move(transaction_origin_in_header));
  }
  return http_context.request->auth_context.authorized_domain;
}

ExecutionResult FrontEndServiceV2::ExecuteConsumeBudgetTransaction(
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>&
        consume_budget_transaction_context) noexcept {
  // No-op. This method is unused.
  return SuccessExecutionResult();
}

ExecutionResult FrontEndServiceV2::BeginTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context, "Start BeginTransaction.");
  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       total_request_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelBeginTransaction,
                                            kMetricNameRequests));
  const std::string reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  const absl::flat_hash_map<std::string, std::string>
      begin_transaction_label_kv = {
          {kMetricLabelTransactionPhase, kMetricLabelBeginTransaction},
          {kMetricLabelKeyReportingOrigin, reporting_origin_metric_label},
      };

  total_request_counter_->Add(1, begin_transaction_label_kv);

  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       client_error_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelBeginTransaction,
                                            kMetricNameClientErrors));
  ExecutionResultOr<std::string> transaction_id =
      ExtractBackwardCompatibleHeaders(
          http_context, /*should_extract_last_execution_timestamp=*/
          false);
  if (!transaction_id.result().Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, begin_transaction_label_kv);
    return transaction_id.result();
  }

  InsertBackwardCompatibleHeaders(http_context);
  http_context.result = SuccessExecutionResult();
  http_context.Finish();

  return SuccessExecutionResult();
}

ExecutionResult FrontEndServiceV2::PrepareTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context,
                    "Start PrepareTransaction.");
  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       total_request_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelPrepareTransaction,
                                            kMetricNameRequests));
  const std::string reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  const absl::flat_hash_map<std::string, std::string>
      prepare_transaction_label_kv = {
          {kMetricLabelTransactionPhase, kMetricLabelPrepareTransaction},
          {kMetricLabelKeyReportingOrigin, reporting_origin_metric_label}};

  total_request_counter_->Add(1, prepare_transaction_label_kv);

  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       client_error_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelPrepareTransaction,
                                            kMetricNameClientErrors));

  ExecutionResultOr<std::string> transaction_id =
      ExtractBackwardCompatibleHeaders(
          http_context, /*should_extract_last_execution_timestamp=*/
          true);
  if (!transaction_id.result().Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, prepare_transaction_label_kv);
    return transaction_id.result();
  }

  AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>
      consume_budget_context(
          std::make_shared<ConsumeBudgetsRequest>(),
          absl::bind_front(&FrontEndServiceV2::OnConsumeBudgetCallback, this,
                           http_context, *transaction_id),
          http_context);
  consume_budget_context.response = std::make_shared<ConsumeBudgetsResponse>();
  auto transaction_origin = ObtainTransactionOrigin(http_context);
  if (auto execution_result = ParseBeginTransactionRequestBody(
          *http_context.request->auth_context.authorized_domain,
          *transaction_origin, http_context.request->body,
          consume_budget_context.request->budgets);
      !execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, prepare_transaction_label_kv);
    return execution_result;
  }

  if (consume_budget_context.request->budgets.size() == 0) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, prepare_transaction_label_kv);
    return FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_NO_KEYS_AVAILABLE);
  }

  if (google::scp::core::common::GlobalLogger::GetGlobalLogger() &&
      google::scp::core::common::GlobalLogger::IsLogLevelEnabled(
          google::scp::core::LogLevel::kDebug)) {
    SCP_DEBUG_CONTEXT(kFrontEndService, http_context,
                      "Starting Transaction: %s Total Keys: %lld",
                      transaction_id->c_str(),
                      consume_budget_context.request->budgets.size());

    for (const auto& consume_budget_metadata :
         consume_budget_context.request->budgets) {
      SCP_DEBUG_CONTEXT(
          kFrontEndService, http_context,
          "Transaction: %s Budget Key: %s Reporting Time Bucket: %llu Token "
          "Count: %d",
          transaction_id->c_str(),
          consume_budget_metadata.budget_key_name->c_str(),
          consume_budget_metadata.time_bucket,
          consume_budget_metadata.token_count);
    }
  }

  if (auto execution_result =
          budget_consumption_helper_->ConsumeBudgets(consume_budget_context);
      !execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, prepare_transaction_label_kv);
    return execution_result;
  }

  return SuccessExecutionResult();
}

void FrontEndServiceV2::OnConsumeBudgetCallback(
    AsyncContext<HttpRequest, HttpResponse> http_context,
    std::string transaction_id,
    AsyncContext<ConsumeBudgetsRequest, ConsumeBudgetsResponse>&
        consume_budget_context) {
  auto server_error_metrics_instance = FindAggregateMetricInMap(
      metrics_instances_map_, kMetricLabelPrepareTransaction,
      kMetricNameServerErrors);

  if (!server_error_metrics_instance.result().Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context,
                      server_error_metrics_instance.result(),
                      "Failed to find server error aggregate metric for "
                      "prepare transaction endpoint.");
    http_context.result = server_error_metrics_instance.result();
    http_context.Finish();
    return;
  }

  auto client_error_metrics_instance = FindAggregateMetricInMap(
      metrics_instances_map_, kMetricLabelPrepareTransaction,
      kMetricNameClientErrors);
  if (!client_error_metrics_instance.result().Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context,
                      server_error_metrics_instance.result(),
                      "Failed to find client error aggregate metric for "
                      "prepare transaction endpoint.");
    http_context.result = client_error_metrics_instance.result();
    http_context.Finish();
    return;
  }

  const std::string reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);

  if (!consume_budget_context.result.Successful()) {
    if (consume_budget_context.result.status_code ==
        errors::SC_CONSUME_BUDGET_EXHAUSTED) {
      SCP_WARNING_CONTEXT(
          kFrontEndService, http_context,
          absl::StrFormat("Failed to consume budget due to budget exhausted. "
                          "transaction_id: %s. execution_result: %s",
                          transaction_id,
                          google::scp::core::errors::GetErrorMessage(
                              consume_budget_context.result.status_code)));
      std::list<size_t> budget_exhausted_indices(
          consume_budget_context.response->budget_exhausted_indices.begin(),
          consume_budget_context.response->budget_exhausted_indices.end());
      auto serialization_execution_result =
          FrontEndUtils::SerializeTransactionFailedCommandIndicesResponse(
              budget_exhausted_indices, http_context.response->body);
      if (!serialization_execution_result.Successful()) {
        // We can log it but should not update the error code getting back
        // to the client since it will make it confusing for the proper
        // diagnosis on the transaction execution errors.
        //
        // This behavior is consistent with front_end_service.cc
        SCP_ERROR_CONTEXT(
            kFrontEndService, http_context, serialization_execution_result,
            absl::StrFormat("Serialization of the transaction response failed. "
                            "transaction_id: %s.",
                            transaction_id));
      }
      (*client_error_metrics_instance)
          ->Increment(reporting_origin_metric_label);
    } else {
      SCP_ERROR_CONTEXT(kFrontEndService, http_context,
                        consume_budget_context.result,
                        "Failed to consume budget. transaction_id: %s.",
                        transaction_id.c_str());
      (*server_error_metrics_instance)
          ->Increment(reporting_origin_metric_label);
    }

    (*server_error_metrics_instance)->Increment(reporting_origin_metric_label);
    server_error_counter_->Add(
        1, {{kMetricLabelTransactionPhase, kMetricLabelPrepareTransaction},
            {kMetricLabelKeyReportingOrigin, reporting_origin_metric_label}});
    http_context.result = consume_budget_context.result;
    http_context.Finish();
    return;
  }

  InsertBackwardCompatibleHeaders(http_context);
  http_context.result = consume_budget_context.result;
  http_context.Finish();
}

[[deprecated(
    "No longer needed in the new version of PBS and will be removed when "
    "clients can no longer rely on this.")]]
ExecutionResult FrontEndServiceV2::CommitTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context, "Start CommitTransaction.");
  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       total_request_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelCommitTransaction,
                                            kMetricNameRequests));
  const std::string reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  const absl::flat_hash_map<std::string, std::string>
      commit_transaction_label_kv = {
          {kMetricLabelTransactionPhase, kMetricLabelCommitTransaction},
          {kMetricLabelKeyReportingOrigin, reporting_origin_metric_label}};

  total_request_counter_->Add(1, commit_transaction_label_kv);

  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       client_error_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelCommitTransaction,
                                            kMetricNameClientErrors));
  ExecutionResultOr<std::string> transaction_id =
      ExtractBackwardCompatibleHeaders(
          http_context, /*should_extract_last_execution_timestamp=*/
          true);
  if (!transaction_id.result().Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, commit_transaction_label_kv);
    return transaction_id.result();
  }

  InsertBackwardCompatibleHeaders(http_context);
  http_context.result = SuccessExecutionResult();
  http_context.Finish();

  return SuccessExecutionResult();
}

[[deprecated(
    "No longer needed in the new version of PBS and will be removed when "
    "clients can no longer rely on this.")]]
ExecutionResult FrontEndServiceV2::NotifyTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context, "Start NotifyTransaction.");
  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       total_request_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelNotifyTransaction,
                                            kMetricNameRequests));
  const std::string reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  const absl::flat_hash_map<std::string, std::string>
      notify_transaction_label_kv = {
          {kMetricLabelTransactionPhase, kMetricLabelNotifyTransaction},
          {kMetricLabelKeyReportingOrigin, reporting_origin_metric_label}};

  total_request_counter_->Add(1, notify_transaction_label_kv);

  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       client_error_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelNotifyTransaction,
                                            kMetricNameClientErrors));
  ExecutionResultOr<std::string> transaction_id =
      ExtractBackwardCompatibleHeaders(
          http_context, /*should_extract_last_execution_timestamp=*/
          true);
  if (!transaction_id.result().Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, notify_transaction_label_kv);
    return transaction_id.result();
  }

  InsertBackwardCompatibleHeaders(http_context);
  http_context.result = SuccessExecutionResult();
  http_context.Finish();

  return SuccessExecutionResult();
}

[[deprecated(
    "No longer needed in the new version of PBS and will be removed when "
    "clients can no longer rely on this.")]] ExecutionResult
FrontEndServiceV2::AbortTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context, "Start AbortTransaction.");
  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       total_request_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelAbortTransaction,
                                            kMetricNameRequests));
  const std::string reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  const absl::flat_hash_map<std::string, std::string>
      abort_transaction_label_kv = {
          {kMetricLabelTransactionPhase, kMetricLabelAbortTransaction},
          {kMetricLabelKeyReportingOrigin, reporting_origin_metric_label}};

  total_request_counter_->Add(1, abort_transaction_label_kv);

  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       client_error_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelAbortTransaction,
                                            kMetricNameClientErrors));
  ExecutionResultOr<std::string> transaction_id =
      ExtractBackwardCompatibleHeaders(
          http_context, /*should_extract_last_execution_timestamp=*/
          true);
  if (!transaction_id.result().Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, abort_transaction_label_kv);
    return transaction_id.result();
  }

  InsertBackwardCompatibleHeaders(http_context);
  http_context.result = SuccessExecutionResult();
  http_context.Finish();

  return SuccessExecutionResult();
}

[[deprecated(
    "No longer needed in the new version of PBS and will be removed when "
    "clients can no longer rely on this.")]] ExecutionResult
FrontEndServiceV2::EndTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context, "Start EndTransaction.");
  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       total_request_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelEndTransaction,
                                            kMetricNameRequests));
  const std::string reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  const absl::flat_hash_map<std::string, std::string> end_transaction_label_kv =
      {{kMetricLabelTransactionPhase, kMetricLabelEndTransaction},
       {kMetricLabelKeyReportingOrigin, reporting_origin_metric_label}};

  total_request_counter_->Add(1, end_transaction_label_kv);

  ASSIGN_OR_RETURN(const std::shared_ptr<AggregateMetricInterface>&
                       client_error_metrics_instance,
                   FindAggregateMetricInMap(metrics_instances_map_,
                                            kMetricLabelEndTransaction,
                                            kMetricNameClientErrors));
  ExecutionResultOr<std::string> transaction_id =
      ExtractBackwardCompatibleHeaders(
          http_context, /*should_extract_last_execution_timestamp=*/
          true);
  if (!transaction_id.result().Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    client_error_counter_->Add(1, end_transaction_label_kv);
    return transaction_id.result();
  }

  InsertBackwardCompatibleHeaders(http_context);
  http_context.result = SuccessExecutionResult();
  http_context.Finish();

  return SuccessExecutionResult();
}

[[deprecated(
    "No longer needed in the new version of PBS and will be removed when "
    "clients can no longer rely on this.")]] ExecutionResult
FrontEndServiceV2::GetTransactionStatus(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context,
                    "Start GetTransactionStatus.");
  return FailureExecutionResult(
      SC_PBS_FRONT_END_SERVICE_GET_TRANSACTION_STATUS_RETURNS_404_BY_DEFAULT);
}
}  // namespace google::scp::pbs
