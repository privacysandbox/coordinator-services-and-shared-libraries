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

#include <chrono>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/http_types.h"
#include "core/interface/transaction_manager_interface.h"
#include "core/interface/type_def.h"
#include "pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "pbs/interface/front_end_service_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::pbs {

core::ExecutionResult ParseBeginTransactionRequestBody(
    const std::string& authorized_domain, const core::BytesBuffer& request_body,
    std::list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
    bool enable_site_based_authorization) noexcept;

class FrontEndUtils {
 public:
  static core::ExecutionResult SerializeTransactionFailedCommandIndicesResponse(
      const std::list<size_t> command_failed_indices,
      core::BytesBuffer& response_body) noexcept {
    try {
      nlohmann::json serialized_body =
          nlohmann::json::parse("{\"v\": \"1.0\"}");
      serialized_body["f"] = nlohmann::json::parse("[]");
      for (auto index : command_failed_indices) {
        serialized_body["f"].push_back(index);
      }
      auto serialized = serialized_body.dump();
      response_body.bytes = std::make_shared<std::vector<core::Byte>>(
          serialized.begin(), serialized.end());
      response_body.length = serialized.size();
      response_body.capacity = serialized.size();
    } catch (...) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY);
    }

    return core::SuccessExecutionResult();
  }

  static core::ExecutionResult SerializePendingTransactionCount(
      const core::GetTransactionManagerStatusResponse& response,
      core::BytesBuffer& response_body) noexcept {
    try {
      nlohmann::json serialized_body =
          nlohmann::json::parse("{\"v\": \"1.0\"}");
      serialized_body["pending_transactions_count"] =
          response.pending_transactions_count;
      auto serialized = serialized_body.dump();
      response_body.bytes = std::make_shared<std::vector<core::Byte>>(
          serialized.begin(), serialized.end());
      response_body.length = serialized.size();
      response_body.capacity = serialized.size();
    } catch (...) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY);
    }

    return core::SuccessExecutionResult();
  }

  static core::ExecutionResult ExtractTransactionId(
      const std::shared_ptr<core::HttpHeaders>& request_headers,
      core::common::Uuid& uuid) noexcept {
    static std::string transaction_id_header(kTransactionIdHeader);

    auto header_iter = request_headers->find(transaction_id_header);
    if (header_iter == request_headers->end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
    }

    return core::common::FromString(header_iter->second, uuid);
  }

  static core::ExecutionResult ExtractRequestClaimedIdentity(
      const std::shared_ptr<core::HttpHeaders>& request_headers,
      std::string& claimed_identity) noexcept {
    if (!request_headers) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
    }
    auto header_iter =
        request_headers->find(std::string(core::kClaimedIdentityHeader));
    if (header_iter == request_headers->end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
    }

    claimed_identity = header_iter->second;
    return core::SuccessExecutionResult();
  }

  static bool IsCoordinatorRequest(
      const std::shared_ptr<core::HttpHeaders>& request_headers,
      const std::string& remote_coordinator_claimed_identity) {
    std::string claimed_identity;
    auto result =
        ExtractRequestClaimedIdentity(request_headers, claimed_identity);
    if (!result.Successful()) {
      SCP_ERROR(
          kFrontEndUtils, core::common::kZeroUuid, result,
          "This could theoretically cause requests with no claimed identity "
          "to be marked as adtech requests. However, this should not be "
          "possible in real-world as all requests hitting the "
          "FrontEndService should have a claimed identity. Without it, they "
          "should not cross the auth barrier.");
      return false;
    }
    return claimed_identity == remote_coordinator_claimed_identity;
  }

  static std::string GetReportingOriginMetricLabel(
      const std::shared_ptr<core::HttpRequest>& request,
      const std::string& remote_coordinator_claimed_identity) {
    bool is_coordinator_request = IsCoordinatorRequest(
        request->headers, remote_coordinator_claimed_identity);
    return is_coordinator_request ? kMetricLabelValueCoordinator
                                  : kMetricLabelValueOperator;
  }

  static core::ExecutionResult ExtractLastExecutionTimestamp(
      const std::shared_ptr<core::HttpHeaders>& request_headers,
      core::Timestamp& timestamp) noexcept {
    static std::string transaction_last_execution_timestamp(
        kTransactionLastExecutionTimestampHeader);

    auto header_iter =
        request_headers->find(transaction_last_execution_timestamp);
    if (header_iter == request_headers->end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
    }

    try {
      if (header_iter->second.length() > 20 ||
          !std::all_of(header_iter->second.begin(), header_iter->second.end(),
                       ::isdigit)) {
        return core::FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
      }

      char* end;
      timestamp = strtoull(header_iter->second.c_str(), &end, 10);
      return core::SuccessExecutionResult();
    } catch (...) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
    }
  }

  static core::ExecutionResult ExtractTransactionSecret(
      const std::shared_ptr<core::HttpHeaders>& request_headers,
      std::string& transaction_secret) noexcept {
    static std::string transaction_secret_header(kTransactionSecretHeader);

    auto header_iter = request_headers->find(transaction_secret_header);
    if (header_iter == request_headers->end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
    }

    transaction_secret = header_iter->second;
    return core::SuccessExecutionResult();
  }

  static core::ExecutionResult ExtractTransactionOrigin(
      const std::shared_ptr<core::HttpHeaders>& request_headers,
      std::string& transaction_origin) noexcept {
    static std::string transaction_origin_header(kTransactionOriginHeader);

    auto header_iter = request_headers->find(transaction_origin_header);
    if (header_iter == request_headers->end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
    }

    transaction_origin = header_iter->second;
    return core::SuccessExecutionResult();
  }

  static core::ExecutionResult DeserializeGetTransactionStatus(
      const core::BytesBuffer& response_body,
      std::shared_ptr<core::GetTransactionStatusResponse>&
          get_transaction_status_response) noexcept {
    try {
      auto get_transaction_status = nlohmann::json::parse(
          response_body.bytes->begin(), response_body.bytes->end());

      if (get_transaction_status.find("is_expired") ==
              get_transaction_status.end() ||
          get_transaction_status.find("has_failures") ==
              get_transaction_status.end() ||
          get_transaction_status.find("last_execution_timestamp") ==
              get_transaction_status.end() ||
          get_transaction_status.find("transaction_execution_phase") ==
              get_transaction_status.end()) {
        return core::FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY);
      }

      get_transaction_status_response->is_expired =
          get_transaction_status["is_expired"].get<bool>();
      get_transaction_status_response->has_failure =
          get_transaction_status["has_failures"].get<bool>();
      get_transaction_status_response->last_execution_timestamp =
          get_transaction_status["last_execution_timestamp"].get<uint64_t>();

      auto transaction_execution_phase =
          get_transaction_status["transaction_execution_phase"]
              .get<std::string>();
      auto execution_result = FromString(
          transaction_execution_phase,
          get_transaction_status_response->transaction_execution_phase);

      if (execution_result != core::SuccessExecutionResult()) {
        return execution_result;
      }
    } catch (...) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY);
    }

    return core::SuccessExecutionResult();
  }

  static core::ExecutionResult SerializeGetTransactionStatus(
      const std::shared_ptr<core::GetTransactionStatusResponse>& response,
      core::BytesBuffer& request_body) noexcept {
    nlohmann::json json_response;
    json_response["is_expired"] = response->is_expired;
    json_response["has_failures"] = response->has_failure;
    json_response["last_execution_timestamp"] =
        response->last_execution_timestamp;

    std::string transaction_execution_phase;
    auto execution_result = ToString(response->transaction_execution_phase,
                                     transaction_execution_phase);
    if (execution_result != core::SuccessExecutionResult()) {
      return execution_result;
    }

    json_response["transaction_execution_phase"] = transaction_execution_phase;
    auto body = json_response.dump();
    request_body.capacity = body.length();
    request_body.length = body.length();
    request_body.bytes =
        std::make_shared<std::vector<core::Byte>>(body.begin(), body.end());
    return core::SuccessExecutionResult();
  }

  static core::ExecutionResult ToString(
      core::TransactionExecutionPhase transaction_execution_phase,
      std::string& output) noexcept {
    switch (transaction_execution_phase) {
      case core::TransactionExecutionPhase::Begin:
        output = "BEGIN";
        return core::SuccessExecutionResult();
      case core::TransactionExecutionPhase::Prepare:
        output = "PREPARE";
        return core::SuccessExecutionResult();
      case core::TransactionExecutionPhase::Commit:
        output = "COMMIT";
        return core::SuccessExecutionResult();
      case core::TransactionExecutionPhase::Notify:
        output = "NOTIFY";
        return core::SuccessExecutionResult();
      case core::TransactionExecutionPhase::Abort:
        output = "ABORT";
        return core::SuccessExecutionResult();
      case core::TransactionExecutionPhase::End:
        output = "END";
        return core::SuccessExecutionResult();
      default:
        output = "UNKNOWN";
        return core::SuccessExecutionResult();
    }
  }

  static core::ExecutionResult FromString(
      std::string& input,
      core::TransactionExecutionPhase& transaction_execution_phase) noexcept {
    if (input.compare("BEGIN") == 0) {
      transaction_execution_phase = core::TransactionExecutionPhase::Begin;
      return core::SuccessExecutionResult();
    }

    if (input.compare("PREPARE") == 0) {
      transaction_execution_phase = core::TransactionExecutionPhase::Prepare;
      return core::SuccessExecutionResult();
    }

    if (input.compare("COMMIT") == 0) {
      transaction_execution_phase = core::TransactionExecutionPhase::Commit;
      return core::SuccessExecutionResult();
    }

    if (input.compare("NOTIFY") == 0) {
      transaction_execution_phase = core::TransactionExecutionPhase::Notify;
      return core::SuccessExecutionResult();
    }

    if (input.compare("ABORT") == 0) {
      transaction_execution_phase = core::TransactionExecutionPhase::Abort;
      return core::SuccessExecutionResult();
    }

    if (input.compare("END") == 0) {
      transaction_execution_phase = core::TransactionExecutionPhase::End;
      return core::SuccessExecutionResult();
    }

    if (input.compare("UNKNOWN") == 0) {
      transaction_execution_phase = core::TransactionExecutionPhase::Unknown;
      return core::SuccessExecutionResult();
    }

    transaction_execution_phase = core::TransactionExecutionPhase::Unknown;
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY);
  }

 private:
  static constexpr char kFrontEndUtils[] = "FrontEndUtils";
};

}  // namespace google::scp::pbs
