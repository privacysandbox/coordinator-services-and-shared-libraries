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

#include "front_end_service.h"

#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/http_types.h"
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/interface/front_end_service_interface.h"
#include "pbs/transactions/src/batch_consume_budget_command.h"
#include "pbs/transactions/src/consume_budget_command.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"
#include "public/cpio/utils/metric_aggregation/src/aggregate_metric.h"

#include "error_codes.h"
#include "front_end_utils.h"

using google::scp::core::AsyncContext;
using google::scp::core::Byte;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionManagerStatusRequest;
using google::scp::core::GetTransactionManagerStatusResponse;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::HttpHandler;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::kAggregatedMetricIntervalMs;
using google::scp::core::kDefaultAggregatedMetricIntervalMs;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::Timestamp;
using google::scp::core::TransactionCommand;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::cpio::AggregateMetric;
using google::scp::cpio::AggregateMetricInterface;
using google::scp::cpio::kCountSecond;
using google::scp::cpio::MetricDefinition;
using google::scp::cpio::MetricLabels;
using google::scp::cpio::MetricLabelsBase;
using google::scp::cpio::MetricName;
using google::scp::cpio::MetricUnit;
using google::scp::pbs::FrontEndUtils;
using std::any_of;
using std::bind;
using std::dynamic_pointer_cast;
using std::list;
using std::make_pair;
using std::make_shared;
using std::map;
using std::move;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;
using std::chrono::milliseconds;
using std::placeholders::_1;

/// TODO: Use configuration service to make the timeout dynamic.
static constexpr size_t kTransactionTimeoutMs = 120 * 1000;
static constexpr char kFrontEndService[] = "FrontEndService";

namespace google::scp::pbs {

FrontEndService::FrontEndService(
    shared_ptr<core::HttpServerInterface>& http_server,
    shared_ptr<core::AsyncExecutorInterface>& async_executor,
    std::unique_ptr<core::TransactionRequestRouterInterface>
        transaction_request_router,
    std::unique_ptr<ConsumeBudgetCommandFactoryInterface>
        consume_budget_command_factory,
    const shared_ptr<cpio::MetricClientInterface>& metric_client,
    const shared_ptr<core::ConfigProviderInterface>& config_provider)
    : http_server_(http_server),
      async_executor_(async_executor),
      transaction_request_router_(move(transaction_request_router)),
      consume_budget_command_factory_(move(consume_budget_command_factory)),
      metric_client_(metric_client),
      config_provider_(config_provider),
      aggregated_metric_interval_ms_(core::kDefaultAggregatedMetricIntervalMs),
      generate_batch_budget_consume_commands_per_day_(false),
      enable_site_based_authorization_(false) {
  if (config_provider_ &&
      !config_provider_->Get(
          core::kPBSAuthorizationEnableSiteBasedAuthorization,
          enable_site_based_authorization_)) {
    enable_site_based_authorization_ = false;
  }
}

core::ExecutionResultOr<std::shared_ptr<cpio::AggregateMetricInterface>>
FrontEndService::RegisterAggregateMetric(const string& name,
                                         const string& phase) noexcept {
  auto metric_name = make_shared<MetricName>(name);
  auto metric_unit = make_shared<MetricUnit>(kCountSecond);
  auto metric_info = make_shared<MetricDefinition>(metric_name, metric_unit);
  MetricLabelsBase label_base(kMetricLabelFrontEndService, phase);
  metric_info->labels =
      make_shared<MetricLabels>(label_base.GetMetricLabelsBase());
  vector<string> labels_list = {kMetricLabelValueOperator,
                                kMetricLabelValueCoordinator};
  shared_ptr<AggregateMetricInterface> metric_instance =
      make_shared<AggregateMetric>(async_executor_, metric_client_, metric_info,
                                   aggregated_metric_interval_ms_,
                                   make_shared<vector<string>>(labels_list),
                                   kMetricLabelKeyReportingOrigin);
  return metric_instance;
}

ExecutionResult FrontEndService::InitMetricInstances() noexcept {
  list<string> method_names = {
      kMetricLabelBeginTransaction,    kMetricLabelPrepareTransaction,
      kMetricLabelCommitTransaction,   kMetricLabelAbortTransaction,
      kMetricLabelNotifyTransaction,   kMetricLabelEndTransaction,
      kMetricLabelGetStatusTransaction};

  list<string> metric_names = {kMetricNameTotalRequest, kMetricNameClientError,
                               kMetricNameServerError};

  for (const auto& method_name : method_names) {
    for (const auto& metric_name : metric_names) {
      auto metric_instance_or =
          RegisterAggregateMetric(metric_name, method_name);
      RETURN_IF_FAILURE(metric_instance_or.result());
      metrics_instances_map_[method_name][metric_name] =
          metric_instance_or.value();
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult FrontEndService::Init() noexcept {
  auto execution_result =
      config_provider_->Get(kEnableBatchBudgetCommandsPerDayConfigName,
                            generate_batch_budget_consume_commands_per_day_);
  if (execution_result != SuccessExecutionResult()) {
    // Config not present, continue with 'false'
    generate_batch_budget_consume_commands_per_day_ = false;
  }
  if (generate_batch_budget_consume_commands_per_day_) {
    SCP_INFO(kFrontEndService, kZeroUuid, "Command batching per day is enabled",
             kEnableBatchBudgetCommandsPerDayConfigName,
             generate_batch_budget_consume_commands_per_day_)
  }

  execution_result =
      config_provider_->Get(kRemotePrivacyBudgetServiceClaimedIdentity,
                            remote_coordinator_claimed_identity_);
  RETURN_IF_FAILURE(execution_result);

  // TODO: It is required to build a better type of versioned resource
  // handling.
  string begin_transaction_path(kBeginTransactionPath);
  HttpHandler begin_transaction_handler =
      bind(&FrontEndService::BeginTransaction, this, _1);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, begin_transaction_path, begin_transaction_handler);

  string prepare_transaction_path(kPrepareTransactionPath);
  HttpHandler prepare_transaction_handler =
      bind(&FrontEndService::PrepareTransaction, this, _1);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, prepare_transaction_path, prepare_transaction_handler);

  string commit_transaction_path(kCommitTransactionPath);
  HttpHandler commit_transaction_handler =
      bind(&FrontEndService::CommitTransaction, this, _1);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, commit_transaction_path, commit_transaction_handler);

  string notify_transaction_path(kNotifyTransactionPath);
  HttpHandler notify_transaction_handler =
      bind(&FrontEndService::NotifyTransaction, this, _1);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, notify_transaction_path, notify_transaction_handler);

  string abort_transaction_path(kAbortTransactionPath);
  HttpHandler abort_transaction_handler =
      bind(&FrontEndService::AbortTransaction, this, _1);
  http_server_->RegisterResourceHandler(
      HttpMethod::POST, abort_transaction_path, abort_transaction_handler);

  string end_transaction_path(kEndTransactionPath);
  HttpHandler end_transaction_handler =
      bind(&FrontEndService::EndTransaction, this, _1);
  http_server_->RegisterResourceHandler(HttpMethod::POST, end_transaction_path,
                                        end_transaction_handler);

  string get_transaction_status_path(kStatusTransactionPath);
  HttpHandler get_transaction_transaction_handler =
      bind(&FrontEndService::GetTransactionStatus, this, _1);
  http_server_->RegisterResourceHandler(HttpMethod::GET,
                                        get_transaction_status_path,
                                        get_transaction_transaction_handler);

  string service_status_path(kServiceStatusPath);
  HttpHandler service_status_handler =
      bind(&FrontEndService::GetServiceStatus, this, _1);
  http_server_->RegisterResourceHandler(HttpMethod::GET, service_status_path,
                                        service_status_handler);

  if (!config_provider_
           ->Get(kAggregatedMetricIntervalMs, aggregated_metric_interval_ms_)
           .Successful()) {
    aggregated_metric_interval_ms_ = kDefaultAggregatedMetricIntervalMs;
  }

  // Initializes TransactionMetrics instances for all transaction phases.
  FrontEndService::InitMetricInstances();
  for (auto const& [method_name, map] : metrics_instances_map_) {
    for (auto const& [metric_name, metric_instance] : map) {
      RETURN_IF_FAILURE(metric_instance->Init());
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult FrontEndService::Run() noexcept {
  // Runs all AggregateMetric instances.
  for (auto const& [method_name, map] : metrics_instances_map_) {
    for (auto const& [metric_name, metric_instance] : map) {
      RETURN_IF_FAILURE(metric_instance->Run());
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult FrontEndService::Stop() noexcept {
  // Shuts down all AggregateMetric instances.
  for (auto const& [method_name, map] : metrics_instances_map_) {
    for (auto const& [metric_name, metric_instance] : map) {
      RETURN_IF_FAILURE(metric_instance->Stop());
    }
  }
  return SuccessExecutionResult();
}

shared_ptr<string> FrontEndService::ObtainTransactionOrigin(
    AsyncContext<HttpRequest, HttpResponse>& http_context) const {
  // If transaction origin is supplied in the header use that instead. The
  // transaction origin in the header is useful if a peer coordinator is
  // resolving a transaction on behalf of a client.
  string transaction_origin_in_header;
  auto execution_result = FrontEndUtils::ExtractTransactionOrigin(
      http_context.request->headers, transaction_origin_in_header);
  if (execution_result.Successful() && !transaction_origin_in_header.empty()) {
    return make_shared<string>(move(transaction_origin_in_header));
  }
  return http_context.request->auth_context.authorized_domain;
}

vector<shared_ptr<TransactionCommand>>
FrontEndService::GenerateConsumeBudgetCommands(
    list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
    const std::string& authorized_domain, const Uuid& transaction_id) {
  vector<shared_ptr<TransactionCommand>> commands;
  size_t index = 0;
  for (auto& budget : consume_budget_metadata_list) {
    std::shared_ptr<std::string> budget_key_name;
    if (enable_site_based_authorization_) {
      budget_key_name = budget.budget_key_name;
    } else {
      budget_key_name = std::make_shared<std::string>(absl::StrCat(
          authorized_domain, string("/"), *budget.budget_key_name));
    }
    ConsumeBudgetCommandRequestInfo budget_info(budget.time_bucket,
                                                budget.token_count, index);
    commands.push_back(consume_budget_command_factory_->ConstructCommand(
        transaction_id, budget_key_name, budget_info));
    index++;
  }
  return commands;
}

struct ConsumeBudgetMetadataTimeBucketOrderingLessThanComparator {
  bool operator()(
      const pair<const ConsumeBudgetMetadata*, ArrayIndex>& left,
      const pair<const ConsumeBudgetMetadata*, ArrayIndex>& right) const {
    return left.first->time_bucket < right.first->time_bucket;
  }
};

vector<shared_ptr<TransactionCommand>>
FrontEndService::GenerateConsumeBudgetCommandsWithBatchesPerDay(
    list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
    const string& authorized_domain, const Uuid& transaction_id) {
  // Populate
  //
  // Format:
  // ------
  // BudgetKey ->
  //    {TimeGroup (day) ->
  //        pair{ConsumeBudgetMetadata, ArrayIndex}}
  //
  // Array Index: Index of the budget item in the request payload.
  //
  map<string,
      map<TimeGroup,
          set<pair<const ConsumeBudgetMetadata*, ArrayIndex>,
              ConsumeBudgetMetadataTimeBucketOrderingLessThanComparator>>>
      budget_key_time_groups_map;
  ArrayIndex array_index = 0;
  // consume_budget_metadata_list is supplied by client, retain its order.
  for (const auto& consume_budget_metadata : consume_budget_metadata_list) {
    // Time group is at day granularity
    auto time_group = budget_key_timeframe_manager::Utils::GetTimeGroup(
        consume_budget_metadata.time_bucket);
    budget_key_time_groups_map[*consume_budget_metadata.budget_key_name]
                              [time_group]
                                  .insert(make_pair(&consume_budget_metadata,
                                                    array_index));
    array_index++;
  }

  // Generate
  vector<shared_ptr<TransactionCommand>> generated_commands;
  for (auto& budget_key_time_groups : budget_key_time_groups_map) {
    auto budget_key_name = std::make_shared<std::string>(absl::StrCat(
        authorized_domain, string("/"), budget_key_time_groups.first));
    if (enable_site_based_authorization_) {
      budget_key_name =
          std::make_shared<std::string>(budget_key_time_groups.first);
    }
    for (auto& time_groups : budget_key_time_groups.second) {
      if (time_groups.second.size() > 1) {
        vector<ConsumeBudgetCommandRequestInfo> budgets;
        for (auto& budget : time_groups.second) {
          budgets.emplace_back(budget.first->time_bucket,
                               budget.first->token_count, budget.second);
        }
        generated_commands.push_back(
            consume_budget_command_factory_->ConstructBatchCommand(
                transaction_id, budget_key_name, std::move(budgets)));
      } else {
        ConsumeBudgetCommandRequestInfo budget(
            (*time_groups.second.begin()).first->time_bucket,
            (*time_groups.second.begin()).first->token_count,
            (*time_groups.second.begin()).second);
        generated_commands.push_back(
            consume_budget_command_factory_->ConstructCommand(
                transaction_id, budget_key_name, std::move(budget)));
      }
    }
  }
  return generated_commands;
}

ExecutionResult FrontEndService::BeginTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  bool disallow_begin_transaction_request = false;
  auto execution_result = config_provider_->Get(
      kDisallowNewTransactionRequests, disallow_begin_transaction_request);
  if (!execution_result.Successful()) {
    disallow_begin_transaction_request = false;
  }

  if (disallow_begin_transaction_request) {
    return FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_BEGIN_TRANSACTION_DISALLOWED);
  }

  const auto& total_request_metrics_instance =
      metrics_instances_map_.at(kMetricLabelBeginTransaction)
          .at(kMetricNameTotalRequest);
  const auto& client_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelBeginTransaction)
          .at(kMetricNameClientError);
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  Uuid transaction_id;
  execution_result = FrontEndUtils::ExtractTransactionId(
      http_context.request->headers, transaction_id);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  string transaction_secret;
  execution_result = FrontEndUtils::ExtractTransactionSecret(
      http_context.request->headers, transaction_secret);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  list<ConsumeBudgetMetadata> consume_budget_metadata_list;
  execution_result = ParseBeginTransactionRequestBody(
      *http_context.request->auth_context.authorized_domain,
      http_context.request->body, consume_budget_metadata_list,
      enable_site_based_authorization_);

  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  if (consume_budget_metadata_list.size() == 0) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_NO_KEYS_AVAILABLE);
  }

  auto transaction_id_string = ToString(transaction_id);
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context,
                    "Starting Transaction: %s Total Keys: %lld",
                    transaction_id_string.c_str(),
                    consume_budget_metadata_list.size());

  auto const& server_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelBeginTransaction)
          .at(kMetricNameServerError);
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context(
      make_shared<TransactionRequest>(),
      bind(&FrontEndService::OnTransactionCallback, this,
           server_error_metrics_instance, http_context, _1),
      http_context);

  // Log the request's budget info
  for (const auto& consume_budget_metadata : consume_budget_metadata_list) {
    std::string budget_key =
        enable_site_based_authorization_
            ? *consume_budget_metadata.budget_key_name
            : absl::StrCat(
                  *http_context.request->auth_context.authorized_domain, "/",
                  *consume_budget_metadata.budget_key_name);
    SCP_DEBUG_CONTEXT(
        kFrontEndService, http_context,
        "Transaction: %s Budget Key: %s Reporting Time Bucket: %llu Token "
        "Count: %d",
        transaction_id_string.c_str(), budget_key.c_str(),
        consume_budget_metadata.time_bucket,
        consume_budget_metadata.token_count);
  }

  if (generate_batch_budget_consume_commands_per_day_) {
    transaction_context.request->commands =
        GenerateConsumeBudgetCommandsWithBatchesPerDay(
            consume_budget_metadata_list,
            *http_context.request->auth_context.authorized_domain,
            transaction_id);
  } else {
    transaction_context.request->commands = GenerateConsumeBudgetCommands(
        consume_budget_metadata_list,
        *http_context.request->auth_context.authorized_domain, transaction_id);
  }

  transaction_context.request->is_coordinated_remotely = true;
  transaction_context.request->transaction_secret =
      make_shared<string>(transaction_secret);
  // Transaction origin during transaction initiation must be the one authorized
  // with the system.
  transaction_context.request->transaction_origin =
      http_context.request->auth_context.authorized_domain;
  transaction_context.request->timeout_time =
      (TimeProvider::GetSteadyTimestampInNanoseconds() +
       milliseconds(kTransactionTimeoutMs))
          .count();
  transaction_context.request->transaction_id = transaction_id;

  execution_result = transaction_request_router_->Execute(transaction_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context, execution_result,
                      "Failed to execute transaction %s",
                      transaction_id_string.c_str());
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
  }
  return execution_result;
}

ExecutionResult FrontEndService::PrepareTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  auto const& total_request_metrics_instance =
      metrics_instances_map_.at(kMetricLabelPrepareTransaction)
          .at(kMetricNameTotalRequest);
  auto const& client_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelPrepareTransaction)
          .at(kMetricNameClientError);
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  Uuid transaction_id;
  auto execution_result = FrontEndUtils::ExtractTransactionId(
      http_context.request->headers, transaction_id);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_secret = make_shared<string>();
  execution_result = FrontEndUtils::ExtractTransactionSecret(
      http_context.request->headers, *transaction_secret);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_origin = ObtainTransactionOrigin(http_context);

  Timestamp last_execution_timestamp;
  execution_result = FrontEndUtils::ExtractLastExecutionTimestamp(
      http_context.request->headers, last_execution_timestamp);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_id_string = ToString(transaction_id);
  SCP_DEBUG_CONTEXT(
      kFrontEndService, http_context,
      "Executing PREPARE phase for transaction: %s LastExecutionTime: %llu",
      transaction_id_string.c_str(), last_execution_timestamp);

  auto const& server_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelPrepareTransaction)
          .at(kMetricNameServerError);
  execution_result = ExecuteTransactionPhase(
      server_error_metrics_instance, http_context, transaction_id,
      transaction_secret, transaction_origin, last_execution_timestamp,
      TransactionExecutionPhase::Prepare);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context, execution_result,
                      "Failed to execute PREPARE phase for transaction: %s",
                      transaction_id_string.c_str());
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
  }
  return execution_result;
}

ExecutionResult FrontEndService::CommitTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  auto const& total_request_metrics_instance =
      metrics_instances_map_.at(kMetricLabelCommitTransaction)
          .at(kMetricNameTotalRequest);
  auto const& client_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelCommitTransaction)
          .at(kMetricNameClientError);
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  Uuid transaction_id;
  auto execution_result = FrontEndUtils::ExtractTransactionId(
      http_context.request->headers, transaction_id);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_secret = make_shared<string>();
  execution_result = FrontEndUtils::ExtractTransactionSecret(
      http_context.request->headers, *transaction_secret);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_origin = ObtainTransactionOrigin(http_context);

  Timestamp last_execution_timestamp;
  execution_result = FrontEndUtils::ExtractLastExecutionTimestamp(
      http_context.request->headers, last_execution_timestamp);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_id_string = ToString(transaction_id);
  SCP_DEBUG_CONTEXT(
      kFrontEndService, http_context,
      "Executing COMMIT phase for transaction: %s LastExecutionTime: %llu",
      transaction_id_string.c_str(), last_execution_timestamp);

  auto const& server_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelCommitTransaction)
          .at(kMetricNameServerError);
  execution_result = ExecuteTransactionPhase(
      server_error_metrics_instance, http_context, transaction_id,
      transaction_secret, transaction_origin, last_execution_timestamp,
      TransactionExecutionPhase::Commit);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context, execution_result,
                      "Failed to execute COMMIT phase for transaction: %s",
                      transaction_id_string.c_str());
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
  }
  return execution_result;
}

ExecutionResult FrontEndService::NotifyTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  auto const& total_request_metrics_instance =
      metrics_instances_map_.at(kMetricLabelNotifyTransaction)
          .at(kMetricNameTotalRequest);
  auto const& client_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelNotifyTransaction)
          .at(kMetricNameClientError);
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  Uuid transaction_id;
  auto execution_result = FrontEndUtils::ExtractTransactionId(
      http_context.request->headers, transaction_id);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_secret = make_shared<string>();
  execution_result = FrontEndUtils::ExtractTransactionSecret(
      http_context.request->headers, *transaction_secret);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_origin = ObtainTransactionOrigin(http_context);

  Timestamp last_execution_timestamp;
  execution_result = FrontEndUtils::ExtractLastExecutionTimestamp(
      http_context.request->headers, last_execution_timestamp);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_id_string = ToString(transaction_id);
  SCP_DEBUG_CONTEXT(
      kFrontEndService, http_context,
      "Executing NOTIFY phase for transaction: %s LastExecutionTime: %llu",
      transaction_id_string.c_str(), last_execution_timestamp);

  auto const& server_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelNotifyTransaction)
          .at(kMetricNameServerError);
  execution_result = ExecuteTransactionPhase(
      server_error_metrics_instance, http_context, transaction_id,
      transaction_secret, transaction_origin, last_execution_timestamp,
      TransactionExecutionPhase::Notify);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context, execution_result,
                      "Failed to execute NOTIFY phase for transaction: %s",
                      transaction_id_string.c_str());
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
  }
  return execution_result;
}

ExecutionResult FrontEndService::AbortTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  auto const& total_request_metrics_instance =
      metrics_instances_map_.at(kMetricLabelAbortTransaction)
          .at(kMetricNameTotalRequest);
  auto const& client_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelAbortTransaction)
          .at(kMetricNameClientError);
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  Uuid transaction_id;
  auto execution_result = FrontEndUtils::ExtractTransactionId(
      http_context.request->headers, transaction_id);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_secret = make_shared<string>();
  execution_result = FrontEndUtils::ExtractTransactionSecret(
      http_context.request->headers, *transaction_secret);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_origin = ObtainTransactionOrigin(http_context);

  Timestamp last_execution_timestamp;
  execution_result = FrontEndUtils::ExtractLastExecutionTimestamp(
      http_context.request->headers, last_execution_timestamp);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_id_string = ToString(transaction_id);
  SCP_DEBUG_CONTEXT(
      kFrontEndService, http_context,
      "Executing ABORT phase for transaction: %s LastExecutionTime: %llu",
      transaction_id_string.c_str(), last_execution_timestamp);

  auto const& server_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelAbortTransaction)
          .at(kMetricNameServerError);
  execution_result = ExecuteTransactionPhase(
      server_error_metrics_instance, http_context, transaction_id,
      transaction_secret, transaction_origin, last_execution_timestamp,
      TransactionExecutionPhase::Abort);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context, execution_result,
                      "Failed to execute ABORT phase for transaction: %s",
                      transaction_id_string.c_str());
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
  }
  return execution_result;
}

ExecutionResult FrontEndService::EndTransaction(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  auto const& total_request_metrics_instance =
      metrics_instances_map_.at(kMetricLabelEndTransaction)
          .at(kMetricNameTotalRequest);
  auto const& client_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelEndTransaction)
          .at(kMetricNameClientError);
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  Uuid transaction_id;
  auto execution_result = FrontEndUtils::ExtractTransactionId(
      http_context.request->headers, transaction_id);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_secret = make_shared<string>();
  execution_result = FrontEndUtils::ExtractTransactionSecret(
      http_context.request->headers, *transaction_secret);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_origin = ObtainTransactionOrigin(http_context);

  Timestamp last_execution_timestamp;
  execution_result = FrontEndUtils::ExtractLastExecutionTimestamp(
      http_context.request->headers, last_execution_timestamp);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_id_string = ToString(transaction_id);
  SCP_DEBUG_CONTEXT(
      kFrontEndService, http_context,
      "Executing END phase for transaction: %s LastExecutionTime: %llu",
      transaction_id_string.c_str(), last_execution_timestamp);

  auto const& server_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelEndTransaction)
          .at(kMetricNameServerError);
  execution_result = ExecuteTransactionPhase(
      server_error_metrics_instance, http_context, transaction_id,
      transaction_secret, transaction_origin, last_execution_timestamp,
      TransactionExecutionPhase::End);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context, execution_result,
                      "Failed to execute END phase for transaction: %s",
                      transaction_id_string.c_str());
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
  }
  return execution_result;
}

ExecutionResult FrontEndService::GetTransactionStatus(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  auto const& total_request_metrics_instance =
      metrics_instances_map_.at(kMetricLabelGetStatusTransaction)
          .at(kMetricNameTotalRequest);
  auto const& client_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelGetStatusTransaction)
          .at(kMetricNameClientError);
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  total_request_metrics_instance->Increment(reporting_origin_metric_label);

  Uuid transaction_id;
  auto execution_result = FrontEndUtils::ExtractTransactionId(
      http_context.request->headers, transaction_id);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  string transaction_secret;
  execution_result = FrontEndUtils::ExtractTransactionSecret(
      http_context.request->headers, transaction_secret);
  if (!execution_result.Successful()) {
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
    return execution_result;
  }

  auto transaction_origin = ObtainTransactionOrigin(http_context);

  auto transaction_id_string = ToString(transaction_id);
  SCP_DEBUG_CONTEXT(kFrontEndService, http_context,
                    "Executing GetTransactionStatus for transaction: %s",
                    transaction_id_string.c_str());

  auto const& server_error_metrics_instance =
      metrics_instances_map_.at(kMetricLabelGetStatusTransaction)
          .at(kMetricNameServerError);
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      get_transaction_status_context(
          make_shared<GetTransactionStatusRequest>(),
          bind(&FrontEndService::OnGetTransactionStatusCallback, this,
               server_error_metrics_instance, http_context, _1),
          http_context);

  get_transaction_status_context.request->transaction_id = transaction_id;
  get_transaction_status_context.request->transaction_secret =
      make_shared<string>(move(transaction_secret));
  get_transaction_status_context.request->transaction_origin =
      transaction_origin;

  execution_result =
      transaction_request_router_->Execute(get_transaction_status_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kFrontEndService, http_context, execution_result,
        "Failed to execute GetTransactionStatus for transaction: %s",
        transaction_id_string.c_str());
    client_error_metrics_instance->Increment(reporting_origin_metric_label);
  }
  return execution_result;
}

/**
 * @brief Returns true any of the provided commands list is a batch
 * command
 *
 * @param commands
 * @return true
 * @return false
 */
bool HasBatchCommand(
    const list<shared_ptr<core::TransactionCommand>>& commands) {
  return any_of(commands.begin(), commands.end(), [](const auto& command) {
    return (dynamic_pointer_cast<BatchConsumeBudgetCommand>(command));
  });
}

/**
 * @brief PopulateInsufficientBudgetsFromBatchCommand
 *
 * Takes a batch command and obtains insufficient budget index (if any)
 * and populates them in the provided failed_budget_consumption_indices
 *
 * @param batch_command
 * @param failed_budget_consumption_indices
 */
void PopulateInsufficientBudgetsFromBatchCommand(
    const BatchConsumeBudgetCommand& batch_command,
    list<size_t>& failed_budget_consumption_indices) {
  const auto& insufficient_budget_consumptions =
      batch_command.GetFailedInsufficientBudgetConsumptions();
  if (insufficient_budget_consumptions.empty()) {
    return;
  }
  for (const auto& insufficient_budget_consumption :
       insufficient_budget_consumptions) {
    if (!insufficient_budget_consumption.request_index.has_value()) {
      continue;
    }
    failed_budget_consumption_indices.push_back(
        *insufficient_budget_consumption.request_index);
  }
}

/**
 * @brief PopulateInsufficientBudgetsFromNonBatchCommand
 *
 * Takes a non-batch command and obtains insufficient budget indicies (if any)
 * and populates them in the provided failed_budget_consumption_indices
 *
 * @param non_batch_command
 * @param failed_budget_consumption_indices
 */
void PopulateInsufficientBudgetsFromNonBatchCommand(
    const ConsumeBudgetCommand& non_batch_command,
    list<size_t>& failed_budget_consumption_indices) {
  const auto& insufficient_budget_consumption =
      non_batch_command.GetFailedInsufficientBudgetConsumption();
  if (!insufficient_budget_consumption.has_value()) {
    return;
  }
  auto request_index = insufficient_budget_consumption->request_index;
  if (!request_index.has_value()) {
    return;
  }
  failed_budget_consumption_indices.push_back(request_index.value());
}

ExecutionResult FrontEndService::GetServiceStatus(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  // TODO: This, for now, assumes that the caller always wants to query
  // Transaction Manager's status, but this can be extended to other
  // components later as needed.

  GetTransactionManagerStatusRequest request;
  GetTransactionManagerStatusResponse response;

  // Serialize pending transactions count from TransactionManager
  auto execution_result =
      transaction_request_router_->Execute(request, response);
  RETURN_IF_FAILURE(execution_result);

  execution_result = FrontEndUtils::SerializePendingTransactionCount(
      response, http_context.response->body);
  RETURN_IF_FAILURE(execution_result);

  // Callback synchronously
  http_context.result = SuccessExecutionResult();
  http_context.Finish();

  return SuccessExecutionResult();
}

/**
 * @brief Serializes the transaction's failed commands on to HTTP response.
 *
 * Two possibilities:
 * 1) If none of the commands are batch, then rely on the
 * failed_commands_indices to construct the response.
 * 2) If atleast one of the commands is a batch command, then use the
 * request_index present in failed_commands themselves.
 *
 * @param failed_commands_indices
 * @param failed_commands
 * @param response_body
 * @return core::ExecutionResult
 */
static core::ExecutionResult SerializeTransactionFailedCommandIndicesResponse(
    const list<size_t>& failed_commands_indices,
    const list<shared_ptr<core::TransactionCommand>>& failed_commands,
    core::BytesBuffer& response_body) noexcept {
  if (!HasBatchCommand(failed_commands)) {
    return FrontEndUtils::SerializeTransactionFailedCommandIndicesResponse(
        failed_commands_indices, response_body);
  }

  // Has atleast one batch command
  if (failed_commands_indices.size() != failed_commands.size()) {
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_BAD_TRANSACTON_COMMANDS);
  }
  list<size_t> failed_budget_consumption_indices;
  for (const auto& failed_command : failed_commands) {
    shared_ptr<BatchConsumeBudgetCommand> failed_batch_command;
    shared_ptr<ConsumeBudgetCommand> failed_non_batch_command;
    if ((failed_batch_command =
             dynamic_pointer_cast<BatchConsumeBudgetCommand>(failed_command))) {
      PopulateInsufficientBudgetsFromBatchCommand(
          *failed_batch_command, failed_budget_consumption_indices);
    } else if ((failed_non_batch_command =
                    dynamic_pointer_cast<ConsumeBudgetCommand>(
                        failed_command))) {
      PopulateInsufficientBudgetsFromNonBatchCommand(
          *failed_non_batch_command, failed_budget_consumption_indices);
    } else {
      // Error. Unknown command. Cast failure.
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_BAD_TRANSACTON_COMMANDS);
    }
  }
  // Sort necessary for the budget order to be sent to client
  failed_budget_consumption_indices.sort();
  return FrontEndUtils::SerializeTransactionFailedCommandIndicesResponse(
      failed_budget_consumption_indices, response_body);
}

void FrontEndService::OnTransactionCallback(
    const shared_ptr<AggregateMetricInterface>& metrics_instance,
    AsyncContext<HttpRequest, HttpResponse>& http_context,
    AsyncContext<TransactionRequest, TransactionResponse>&
        transaction_context) noexcept {
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  if (!transaction_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context,
                      transaction_context.result,
                      "Transaction callback failed.");

    if (transaction_context.result.status == core::ExecutionStatus::Failure &&
        transaction_context.response) {
      auto local_execution_result =
          SerializeTransactionFailedCommandIndicesResponse(
              transaction_context.response->failed_commands_indices,
              transaction_context.response->failed_commands,
              http_context.response->body);
      if (!local_execution_result.Successful()) {
        // We can log it but should not update the error code getting back to
        // the client since it will make it confusing for the proper diagnosis
        // on the transaction execution errors.
        SCP_ERROR_CONTEXT(kFrontEndService, http_context,
                          local_execution_result,
                          "Serialization of the transaction response failed");
      }
    }

    http_context.result = transaction_context.result;
    http_context.Finish();
    metrics_instance->Increment(reporting_origin_metric_label);
    return;
  }

  string uuid_string = ToString(transaction_context.response->transaction_id);
  static string transaction_id_header(kTransactionIdHeader);

  http_context.response->headers->insert({transaction_id_header, uuid_string});

  SCP_DEBUG_CONTEXT(
      kFrontEndService, http_context,
      "Executing BEGIN phase for transaction: %s LastExecutionTime: %llu",
      uuid_string.c_str(),
      transaction_context.response->last_execution_timestamp);

  auto execution_result = ExecuteTransactionPhase(
      metrics_instance, http_context,
      transaction_context.request->transaction_id,
      transaction_context.request->transaction_secret,
      transaction_context.request->transaction_origin,
      transaction_context.response->last_execution_timestamp,
      TransactionExecutionPhase::Begin);

  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context, execution_result,
                      "Execute BEGIN phase failed for transaction: %s",
                      uuid_string.c_str());
    http_context.result = execution_result;
    http_context.Finish();
    metrics_instance->Increment(reporting_origin_metric_label);
    return;
  }
}

ExecutionResult FrontEndService::ExecuteTransactionPhase(
    const shared_ptr<AggregateMetricInterface>& metrics_instance,
    AsyncContext<HttpRequest, HttpResponse>& http_context, Uuid& transaction_id,
    shared_ptr<string>& transaction_secret,
    shared_ptr<string>& transaction_origin,
    Timestamp last_transaction_execution_timestamp,
    TransactionExecutionPhase transaction_execution_phase) noexcept {
  AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>
      transaction_phase_context(
          make_shared<TransactionPhaseRequest>(),
          bind(&FrontEndService::OnExecuteTransactionPhaseCallback, this,
               metrics_instance, http_context, _1),
          http_context);
  transaction_phase_context.request->transaction_execution_phase =
      transaction_execution_phase;
  transaction_phase_context.request->transaction_id = transaction_id;
  transaction_phase_context.request->last_execution_timestamp =
      last_transaction_execution_timestamp;
  transaction_phase_context.request->transaction_secret = transaction_secret;
  transaction_phase_context.request->transaction_origin = transaction_origin;
  return transaction_request_router_->Execute(transaction_phase_context);
}

void FrontEndService::OnExecuteTransactionPhaseCallback(
    const shared_ptr<AggregateMetricInterface>& metrics_instance,
    AsyncContext<HttpRequest, HttpResponse>& http_context,
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        transaction_phase_context) noexcept {
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);

  auto transaction_id_string =
      ToString(transaction_phase_context.request->transaction_id);

  if (!transaction_phase_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kFrontEndService, http_context,
                      transaction_phase_context.result,
                      "Transaction phase execution failed for transaction: %s",
                      transaction_id_string.c_str());

    metrics_instance->Increment(reporting_origin_metric_label);
  }

  if (transaction_phase_context.result.Successful()) {
    static string transaction_last_execution_timestamp(
        kTransactionLastExecutionTimestampHeader);

    http_context.response->headers->insert(
        {transaction_last_execution_timestamp,
         to_string(
             transaction_phase_context.response->last_execution_timestamp)});

  } else if (transaction_phase_context.result.status ==
                 core::ExecutionStatus::Failure &&
             transaction_phase_context.response) {
    auto local_execution_result =
        SerializeTransactionFailedCommandIndicesResponse(
            transaction_phase_context.response->failed_commands_indices,
            transaction_phase_context.response->failed_commands,
            http_context.response->body);
    if (!local_execution_result.Successful()) {
      // We can log it but should not update the error code getting back to
      // the client since it will make it confusing for the proper diagnosis
      // on the transaction execution errors.
      SCP_ERROR_CONTEXT(
          kFrontEndService, http_context, local_execution_result,
          "Serialization of the transaction phase response failed "
          "for transaction: %s",
          transaction_id_string.c_str());
    }
  }

  SCP_DEBUG_CONTEXT(
      kFrontEndService, transaction_phase_context,
      "Transaction phase execution completed for transaction: %s, "
      "TransactionExecutionPhase enum: %d, LastExecutionTimestamp: "
      "%lld, HTTP Response: %s",
      transaction_id_string.c_str(),
      transaction_phase_context.request->transaction_execution_phase,
      transaction_phase_context.request->last_execution_timestamp,
      http_context.response->body.ToString().c_str());

  http_context.result = transaction_phase_context.result;
  http_context.Finish();
}

void FrontEndService::OnGetTransactionStatusCallback(
    const shared_ptr<AggregateMetricInterface>& metrics_instance,
    AsyncContext<HttpRequest, HttpResponse>& http_context,
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context) noexcept {
  auto transaction_id_string =
      ToString(get_transaction_status_context.request->transaction_id);
  const string& reporting_origin_metric_label =
      FrontEndUtils::FrontEndUtils::GetReportingOriginMetricLabel(
          http_context.request, remote_coordinator_claimed_identity_);
  http_context.result = get_transaction_status_context.result;
  if (!get_transaction_status_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kFrontEndService, http_context, get_transaction_status_context.result,
        "Get transaction status callback failed for transaction: %s",
        transaction_id_string.c_str());
    http_context.Finish();
    metrics_instance->Increment(reporting_origin_metric_label);
    return;
  }

  auto execution_result = FrontEndUtils::SerializeGetTransactionStatus(
      get_transaction_status_context.response, http_context.response->body);
  if (!execution_result.Successful()) {
    http_context.result = execution_result;
    http_context.Finish();
    metrics_instance->Increment(reporting_origin_metric_label);
    return;
  }

  SCP_DEBUG_CONTEXT(kFrontEndService, get_transaction_status_context,
                    "Transaction GetTransactionManagerStatus completed for "
                    "transaction: %s, HTTP Response: %s",
                    transaction_id_string.c_str(),
                    http_context.response->body.ToString().c_str());

  http_context.Finish();
}

ExecutionResult FrontEndService::ExecuteConsumeBudgetTransaction(
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>&
        consume_budget_transaction_context) noexcept {
  if (consume_budget_transaction_context.request->budget_keys->size() == 0) {
    return FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
  }
  AsyncContext<TransactionRequest, TransactionResponse> transaction_context(
      make_shared<TransactionRequest>(),
      bind(&FrontEndService::OnExecuteConsumeBudgetTransactionCallback, this,
           consume_budget_transaction_context, _1),
      consume_budget_transaction_context);

  auto transaction_id = Uuid::GenerateUuid();
  for (auto& budget_key :
       *consume_budget_transaction_context.request->budget_keys) {
    // Request index is not supplied here since this method is not invoked by
    // clients.
    transaction_context.request->commands.push_back(
        consume_budget_command_factory_->ConstructCommand(
            transaction_id, budget_key.budget_key_name,
            ConsumeBudgetCommandRequestInfo(budget_key.time_bucket,
                                            budget_key.token_count)));
  }

  // The transaction is coordinated/orchestrated end-to-end by the Transaction
  // Manager.
  transaction_context.request->is_coordinated_remotely = false;
  transaction_context.request->timeout_time =
      (TimeProvider::GetSteadyTimestampInNanoseconds() +
       milliseconds(kTransactionTimeoutMs))
          .count();
  transaction_context.request->transaction_id = transaction_id;
  return transaction_request_router_->Execute(transaction_context);
}

void FrontEndService::OnExecuteConsumeBudgetTransactionCallback(
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>&
        consume_budget_transaction_context,
    AsyncContext<TransactionRequest, TransactionResponse>&
        transaction_context) noexcept {
  consume_budget_transaction_context.result = transaction_context.result;
  consume_budget_transaction_context.Finish();
}
}  // namespace google::scp::pbs
