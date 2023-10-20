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

#include "pbs_client.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <nlohmann/json.hpp>

#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/authorization_service_interface.h"
#include "core/interface/type_def.h"
#include "pbs/front_end_service/src/front_end_utils.h"
#include "pbs/interface/type_def.h"

#include "error_codes.h"

using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::TransactionExecutionPhase;
using google::scp::core::TransactionPhaseRequest;
using google::scp::core::TransactionPhaseResponse;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using google::scp::pbs::FrontEndUtils;
using std::bind;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::unique_lock;
using std::vector;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using std::placeholders::_1;

namespace google::scp::pbs {
PrivacyBudgetServiceClient::PrivacyBudgetServiceClient(
    const string& reporting_origin, const string& pbs_endpoint,
    const shared_ptr<HttpClientInterface>& http_client,
    const std::shared_ptr<core::TokenProviderCacheInterface>&
        authorization_token_provider_cache)
    : reporting_origin_(reporting_origin),
      pbs_endpoint_(pbs_endpoint),
      http_client_(http_client),
      authorization_token_provider_cache_(authorization_token_provider_cache) {
  get_transaction_status_url_ =
      make_shared<string>(pbs_endpoint_ + string(kStatusTransactionPath));
  begin_consume_budget_transaction_url_ =
      make_shared<string>(pbs_endpoint_ + string(kBeginTransactionPath));
  prepare_consume_budget_transaction_url_ =
      make_shared<string>(pbs_endpoint_ + string(kPrepareTransactionPath));
  commit_consume_budget_transaction_url_ =
      make_shared<string>(pbs_endpoint_ + string(kCommitTransactionPath));
  notify_consume_budget_transaction_url_ =
      make_shared<string>(pbs_endpoint_ + string(kNotifyTransactionPath));
  abort_consume_budget_transaction_url_ =
      make_shared<string>(pbs_endpoint_ + string(kAbortTransactionPath));
  end_consume_budget_transaction_url_ =
      make_shared<string>(pbs_endpoint_ + string(kEndTransactionPath));
}

ExecutionResult PrivacyBudgetServiceClient::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivacyBudgetServiceClient::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivacyBudgetServiceClient::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult
PrivacyBudgetServiceClient::SerializeConsumeBudgetTransactionRequest(
    shared_ptr<ConsumeBudgetTransactionRequest>&
        consume_budget_transaction_request,
    string& serialized) noexcept {
  if (!consume_budget_transaction_request ||
      !consume_budget_transaction_request->budget_keys ||
      consume_budget_transaction_request->budget_keys->size() == 0) {
    return FailureExecutionResult(
        core::errors::SC_PBS_CLIENT_NO_BUDGET_KEY_PROVIDED);
  }

  nlohmann::json serialized_body =
      nlohmann::json::parse("{\"v\": \"1.0\", \"t\": []}");
  for (auto budget_key : *consume_budget_transaction_request->budget_keys) {
    nlohmann::json json_budget_key;
    json_budget_key["key"] = *budget_key.budget_key_name;
    json_budget_key["token"] = budget_key.token_count;

    seconds second_count = std::chrono::duration_cast<seconds>(
        nanoseconds(budget_key.time_bucket));
    google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(second_count.count());

    auto rfc_time = TimeUtil::ToString(timestamp);
    if (rfc_time.size() == 0) {
      return FailureExecutionResult(
          core::errors::SC_PBS_CLIENT_INVALID_TRANSACTION_METADATA);
    }
    json_budget_key["reporting_time"] = rfc_time;
    serialized_body["t"].push_back(json_budget_key);
  }
  serialized = serialized_body.dump();
  return SuccessExecutionResult();
}

ExecutionResult PrivacyBudgetServiceClient::GetTransactionStatus(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context) noexcept {
  AsyncContext<HttpRequest, HttpResponse> http_context(
      make_shared<HttpRequest>(),
      bind(&PrivacyBudgetServiceClient::OnGetTransactionStatusCallback, this,
           get_transaction_status_context, _1),
      get_transaction_status_context);

  string transaction_id =
      ToString(get_transaction_status_context.request->transaction_id);

  auto auth_token_or = authorization_token_provider_cache_->GetToken();
  if (!auth_token_or.Successful()) {
    return auth_token_or.result();
  }

  http_context.request->path = get_transaction_status_url_;
  http_context.request->method = HttpMethod::GET;
  http_context.request->body.length = 0;
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {string(core::kAuthHeader), *auth_token_or.value()});
  http_context.request->headers->insert(
      {string(core::kClaimedIdentityHeader), reporting_origin_});
  http_context.request->headers->insert(
      {string(kTransactionIdHeader), transaction_id});
  http_context.request->headers->insert(
      {string(kTransactionSecretHeader),
       *get_transaction_status_context.request->transaction_secret});

  // Transaction origin is optional field and is supplied when a coordinator is
  // acting on bahalf of a remotely coordinated transaction
  if (get_transaction_status_context.request->transaction_origin) {
    http_context.request->headers->insert(
        {string(kTransactionOriginHeader),
         *get_transaction_status_context.request->transaction_origin});
  }

  return http_client_->PerformRequest(http_context);
}

void PrivacyBudgetServiceClient::OnGetTransactionStatusCallback(
    AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>&
        get_transaction_status_context,
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  // TODO: Update the error code for not found transactions.
  if (!http_context.result.Successful()) {
    get_transaction_status_context.result = http_context.result;
    get_transaction_status_context.Finish();
    return;
  }

  get_transaction_status_context.response =
      make_shared<GetTransactionStatusResponse>();
  auto execution_result = FrontEndUtils::DeserializeGetTransactionStatus(
      http_context.response->body, get_transaction_status_context.response);
  get_transaction_status_context.result = execution_result;
  get_transaction_status_context.Finish();
}

ExecutionResult PrivacyBudgetServiceClient::InitiateConsumeBudgetTransaction(
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>&
        consume_budget_transaction_context) noexcept {
  string serialized_body;
  auto execution_result = SerializeConsumeBudgetTransactionRequest(
      consume_budget_transaction_context.request, serialized_body);
  if (!execution_result.Successful()) {
    return execution_result;
  }

  AsyncContext<HttpRequest, HttpResponse> http_context(
      make_shared<HttpRequest>(),
      bind(&PrivacyBudgetServiceClient::
               OnInitiateConsumeBudgetTransactionCallback,
           this, consume_budget_transaction_context, _1),
      consume_budget_transaction_context);

  http_context.request->path = begin_consume_budget_transaction_url_;
  http_context.request->body = BytesBuffer(serialized_body.length());
  http_context.request->body.bytes =
      make_shared<vector<Byte>>(serialized_body.begin(), serialized_body.end());
  http_context.request->body.length = serialized_body.length();

  auto auth_token_or = authorization_token_provider_cache_->GetToken();
  if (!auth_token_or.Successful()) {
    return auth_token_or.result();
  }

  http_context.request->method = HttpMethod::POST;
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {string(core::kAuthHeader), *auth_token_or.value()});
  http_context.request->headers->insert(
      {string(core::kClaimedIdentityHeader), reporting_origin_});
  http_context.request->headers->insert(
      {string(kTransactionIdHeader),
       core::common::ToString(
           consume_budget_transaction_context.request->transaction_id)});
  http_context.request->headers->insert(
      {string(kTransactionSecretHeader),
       *consume_budget_transaction_context.request->transaction_secret});

  return http_client_->PerformRequest(http_context);
}

void PrivacyBudgetServiceClient::OnInitiateConsumeBudgetTransactionCallback(
    AsyncContext<ConsumeBudgetTransactionRequest,
                 ConsumeBudgetTransactionResponse>&
        consume_budget_transaction_context,
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  if (!http_context.result.Successful()) {
    consume_budget_transaction_context.result = http_context.result;
    consume_budget_transaction_context.Finish();
    return;
  }

  static std::string transaction_last_execution_timestamp_header(
      kTransactionLastExecutionTimestampHeader);

  auto header_iter = http_context.response->headers->find(
      transaction_last_execution_timestamp_header);
  if (header_iter == http_context.response->headers->end()) {
    consume_budget_transaction_context.result = FailureExecutionResult(
        core::errors::SC_PBS_CLIENT_RESPONSE_HEADER_NOT_FOUND);
    consume_budget_transaction_context.Finish();
    return;
  }

  auto last_execution_timestamp = header_iter->second;
  consume_budget_transaction_context.response =
      make_shared<ConsumeBudgetTransactionResponse>();

  ExecutionResult execution_result = ParseTransactionLastExecutionTime(
      last_execution_timestamp,
      consume_budget_transaction_context.response->last_execution_timestamp);

  consume_budget_transaction_context.result = execution_result;
  consume_budget_transaction_context.Finish();
}

ExecutionResult PrivacyBudgetServiceClient::ExecuteTransactionPhase(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        execute_transaction_phase_context) noexcept {
  AsyncContext<HttpRequest, HttpResponse> http_context(
      make_shared<HttpRequest>(),
      bind(&PrivacyBudgetServiceClient::OnExecuteTransactionPhaseCallback, this,
           execute_transaction_phase_context, _1),
      execute_transaction_phase_context);

  string transaction_id =
      ToString(execute_transaction_phase_context.request->transaction_id);

  http_context.request->method = HttpMethod::POST;
  switch (
      execute_transaction_phase_context.request->transaction_execution_phase) {
    case TransactionExecutionPhase::Begin:
      http_context.request->path = begin_consume_budget_transaction_url_;
      break;
    case TransactionExecutionPhase::Prepare:
      http_context.request->path = prepare_consume_budget_transaction_url_;
      break;
    case TransactionExecutionPhase::Commit:
      http_context.request->path = commit_consume_budget_transaction_url_;
      break;
    case TransactionExecutionPhase::Notify:
      http_context.request->path = notify_consume_budget_transaction_url_;
      break;
    case TransactionExecutionPhase::Abort:
      http_context.request->path = abort_consume_budget_transaction_url_;
      break;
    case TransactionExecutionPhase::End:
      http_context.request->path = end_consume_budget_transaction_url_;
      break;
    default:
      return FailureExecutionResult(core::errors::SC_PBS_CLIENT_INVALID_PHASE);
  }

  auto auth_token_or = authorization_token_provider_cache_->GetToken();
  if (!auth_token_or.Successful()) {
    return auth_token_or.result();
  }

  http_context.request->body.length = 0;
  http_context.request->headers = make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {string(core::kAuthHeader), *auth_token_or.value()});
  http_context.request->headers->insert(
      {string(core::kClaimedIdentityHeader), reporting_origin_});
  http_context.request->headers->insert(
      {string(kTransactionIdHeader), transaction_id});
  http_context.request->headers->insert(
      {string(kTransactionSecretHeader),
       *execute_transaction_phase_context.request->transaction_secret});

  // Transaction origin is optional field and is supplied when a coordinator
  // is acting on bahalf of a remotely coordinated transaction
  if (execute_transaction_phase_context.request->transaction_origin) {
    http_context.request->headers->insert(
        {string(kTransactionOriginHeader),
         *execute_transaction_phase_context.request->transaction_origin});
  }

  http_context.request->headers->insert(
      {string(kTransactionLastExecutionTimestampHeader),
       std::to_string(execute_transaction_phase_context.request
                          ->last_execution_timestamp)});

  return http_client_->PerformRequest(http_context);
}

void PrivacyBudgetServiceClient::OnExecuteTransactionPhaseCallback(
    AsyncContext<TransactionPhaseRequest, TransactionPhaseResponse>&
        execute_transaction_phase_context,
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  if (!http_context.result.Successful()) {
    execute_transaction_phase_context.result = http_context.result;
    execute_transaction_phase_context.Finish();
    return;
  }

  auto last_execution_timestamp_iter = http_context.response->headers->find(
      string(kTransactionLastExecutionTimestampHeader));

  if (last_execution_timestamp_iter == http_context.response->headers->end()) {
    execute_transaction_phase_context.result = FailureExecutionResult(
        core::errors::SC_PBS_CLIENT_RESPONSE_HEADER_NOT_FOUND);
    execute_transaction_phase_context.Finish();
    return;
  }

  execute_transaction_phase_context.response =
      make_shared<TransactionPhaseResponse>();
  execute_transaction_phase_context.result = ParseTransactionLastExecutionTime(
      last_execution_timestamp_iter->second,
      execute_transaction_phase_context.response->last_execution_timestamp);
  execute_transaction_phase_context.Finish();
}

ExecutionResult PrivacyBudgetServiceClient::ParseTransactionLastExecutionTime(
    string& transaction_last_execution_time_str,
    Timestamp& transaction_last_execution_time) noexcept {
  try {
    // Length = 20 is not modifiable. The maximum length of timestamp is
    // uint64_t string length.
    if (transaction_last_execution_time_str.length() > 20 ||
        !std::all_of(transaction_last_execution_time_str.begin(),
                     transaction_last_execution_time_str.end(), ::isdigit)) {
      return FailureExecutionResult(
          core::errors::SC_PBS_CLIENT_INVALID_RESPONSE_HEADER);
    } else {
      char* end;
      transaction_last_execution_time =
          strtoull(transaction_last_execution_time_str.c_str(), &end, 10);
    }
  } catch (...) {
    return FailureExecutionResult(
        core::errors::SC_PBS_CLIENT_INVALID_RESPONSE_HEADER);
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::pbs
