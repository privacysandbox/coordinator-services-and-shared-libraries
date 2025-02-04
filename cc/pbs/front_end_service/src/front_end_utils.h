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

#include <list>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <nlohmann/json.hpp>

#include "absl/container/flat_hash_map.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "opentelemetry/common/key_value_iterable_view.h"

namespace google::scp::pbs {

core::ExecutionResult ParseBeginTransactionRequestBody(
    const std::string& authorized_domain, const core::BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) noexcept;

core::ExecutionResult ParseBeginTransactionRequestBody(
    const std::string& authorized_domain, const std::string& transaction_origin,
    const core::BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) noexcept;

core::ExecutionResultOr<std::string> TransformReportingOriginToSite(
    const std::string& reporting_origin);

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

  static opentelemetry::common::KeyValueIterableView<
      absl::flat_hash_map<std::string, std::string>>
  CreateMetricLabelsKV(
      const absl::flat_hash_map<std::string, std::string>& metric_labels) {
    return opentelemetry::common::KeyValueIterableView<
        absl::flat_hash_map<std::string, std::string>>(metric_labels);
  }

 private:
  static constexpr char kFrontEndUtils[] = "FrontEndUtils";
};

}  // namespace google::scp::pbs
