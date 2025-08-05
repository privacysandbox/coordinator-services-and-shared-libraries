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

#include "cc/pbs/front_end_service/src/front_end_utils.h"

#include <libpsl.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/time_util.h>
#include <nlohmann/json.hpp>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs {

namespace {

using ::google::protobuf::util::JsonPrintOptions;
using ::google::protobuf::util::MessageToJsonString;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest;
using ::privacy_sandbox::pbs::v1::ConsumePrivacyBudgetResponse;
using ::privacy_sandbox::pbs_common::Byte;
using ::privacy_sandbox::pbs_common::BytesBuffer;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::ExecutionResultOr;
using ::privacy_sandbox::pbs_common::FailureExecutionResult;
using ::privacy_sandbox::pbs_common::HttpHeaders;
using ::privacy_sandbox::pbs_common::kClaimedIdentityHeader;
using ::privacy_sandbox::pbs_common::kZeroUuid;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using ::privacy_sandbox::pbs_common::Uuid;

constexpr absl::string_view kFrontEndUtils = "FrontEndUtils";
constexpr absl::string_view kVersion1 = "1.0";
constexpr absl::string_view kVersion2 = "2.0";
constexpr absl::string_view kHttpPrefix = "http://";
constexpr absl::string_view kHttpsPrefix = "https://";

ExecutionResultOr<TimeBucket> ReportingTimeToTimeBucket(
    const std::string& reporting_time) {
  google::protobuf::Timestamp reporting_timestamp;
  if (!google::protobuf::util::TimeUtil::FromString(reporting_time,
                                                    &reporting_timestamp)) {
    return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
  }

  if (reporting_timestamp.seconds() < 0) {
    return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
  }

  uint64_t seconds_since_epoch = reporting_timestamp.seconds();
  std::chrono::nanoseconds reporting_time_nanoseconds =
      std::chrono::seconds(seconds_since_epoch);
  return static_cast<uint64_t>(reporting_time_nanoseconds.count());
}

}  // namespace

ExecutionResult SerializeTransactionFailedCommandIndicesResponse(
    const std::vector<size_t> command_failed_indices,
    BytesBuffer& response_body) {
  std::string serialized;
  ConsumePrivacyBudgetResponse response_proto;
  response_proto.set_version(kVersion1);
  response_proto.mutable_exhausted_budget_indices()->Assign(
      command_failed_indices.begin(), command_failed_indices.end());

  JsonPrintOptions json_print_options;
  json_print_options.always_print_fields_with_no_presence = true;

  if (auto status =
          MessageToJsonString(response_proto, &serialized, json_print_options);
      !status.ok()) {
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY);
  }

  response_body.bytes =
      std::make_shared<std::vector<Byte>>(serialized.begin(), serialized.end());
  response_body.length = serialized.size();
  response_body.capacity = serialized.size();

  return SuccessExecutionResult();
}

ExecutionResult ExtractTransactionIdFromHTTPHeaders(
    const std::shared_ptr<HttpHeaders>& request_headers, Uuid& uuid) {
  auto header_iter = request_headers->find(kTransactionIdHeader);
  if (header_iter == request_headers->end()) {
    return FailureExecutionResult(
        privacy_sandbox::pbs::
            SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
  }

  return FromString(header_iter->second, uuid);
}

ExecutionResult ExtractRequestClaimedIdentity(
    const std::shared_ptr<HttpHeaders>& request_headers,
    std::string& claimed_identity) {
  if (!request_headers) {
    return FailureExecutionResult(
        privacy_sandbox::pbs::
            SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
  }
  auto header_iter = request_headers->find(std::string(kClaimedIdentityHeader));
  if (header_iter == request_headers->end()) {
    return FailureExecutionResult(
        privacy_sandbox::pbs::
            SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
  }

  claimed_identity = header_iter->second;
  return SuccessExecutionResult();
}

std::string GetReportingOriginMetricLabel() {
  return kMetricLabelValueOperator;
}

ExecutionResultOr<std::string> ExtractTransactionOrigin(
    const HttpHeaders& request_headers) {
  auto header_iter = request_headers.find(kTransactionOriginHeader);
  if (header_iter == request_headers.end()) {
    return FailureExecutionResult(
        privacy_sandbox::pbs::
            SC_PBS_FRONT_END_SERVICE_REQUEST_HEADER_NOT_FOUND);
  }
  return header_iter->second;
}

ExecutionResultOr<std::string> TransformReportingOriginToSite(
    const std::string& reporting_origin) {
  const psl_ctx_t* psl = psl_builtin();
  const char* private_suffix_part_start =
      psl_registrable_domain(psl, reporting_origin.c_str());
  if (private_suffix_part_start == nullptr) {
    return FailureExecutionResult(
        privacy_sandbox::pbs::
            SC_PBS_FRONT_END_SERVICE_INVALID_REPORTING_ORIGIN);
  }
  std::string private_suffix_part = std::string(private_suffix_part_start);
  // Extract port number after the address portion (skipping protocol scheme).
  size_t port_idx =
      private_suffix_part.find(":", private_suffix_part.find("."));
  if (port_idx != std::string::npos) {
    // Remove a port number if exists.
    private_suffix_part = private_suffix_part.substr(0, port_idx);
  }
  // Remove trailing slash (/) if it exists
  size_t trailing_slash_idx =
      private_suffix_part.find("/", private_suffix_part.find("."));
  if (trailing_slash_idx != std::string::npos) {
    private_suffix_part = private_suffix_part.substr(0, trailing_slash_idx);
  }
  if (absl::StartsWith(private_suffix_part, kHttpsPrefix)) {
    return private_suffix_part;
  }
  if (absl::StartsWith(private_suffix_part, kHttpPrefix)) {
    return absl::StrCat(kHttpsPrefix,
                        absl::StripPrefix(private_suffix_part, kHttpPrefix));
  }
  return absl::StrCat(kHttpsPrefix, private_suffix_part);
}

ExecutionResultOr<ConsumePrivacyBudgetRequest::PrivacyBudgetKey::BudgetType>
ValidateAndGetBudgetType(const ConsumePrivacyBudgetRequest& request_proto) {
  if (request_proto.version() != kVersion2) {
    SCP_INFO(kFrontEndUtils, kZeroUuid,
             absl::StrCat("Proto must have version 2.0, found ",
                          request_proto.version()));
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  using PrivacyBudgetKey = ConsumePrivacyBudgetRequest::PrivacyBudgetKey;
  PrivacyBudgetKey::BudgetType budget_type =
      PrivacyBudgetKey::BUDGET_TYPE_UNSPECIFIED;

  for (const auto& data_body : request_proto.data()) {
    for (const auto& key_body : data_body.keys()) {
      auto key_budget_type = key_body.budget_type();
      if (key_budget_type == PrivacyBudgetKey::BUDGET_TYPE_UNSPECIFIED) {
        // Default is binary budget consumer
        key_budget_type = PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET;
      }

      // All keys should have the same budget type.
      if (budget_type == PrivacyBudgetKey::BUDGET_TYPE_UNSPECIFIED) {
        budget_type = key_budget_type;
      } else if (budget_type != key_budget_type) {
        SCP_INFO(kFrontEndUtils, kZeroUuid,
                 absl::StrFormat(
                     "All keys should have the same budget type. "
                     "Expected %s Found %s",
                     PrivacyBudgetKey::BudgetType_Name(budget_type),
                     PrivacyBudgetKey::BudgetType_Name(key_budget_type)));
        return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
      }
    }
  }

  // Default is binary budget consumer
  return budget_type == PrivacyBudgetKey::BUDGET_TYPE_UNSPECIFIED
             ? PrivacyBudgetKey::BUDGET_TYPE_BINARY_BUDGET
             : budget_type;
}

ExecutionResult ParseCommonV2TransactionRequestProto(
    absl::string_view authorized_domain,
    const privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest& request_proto,
    ProtoKeyBodyProcesserFunction key_body_processer) {
  if (request_proto.version() != kVersion2) {
    SCP_INFO(kFrontEndUtils, kZeroUuid, "Not a version 2.0 request");
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  absl::flat_hash_set<std::string> visited_reporting_origin;
  size_t key_index = 0;

  for (const auto& data_body : request_proto.data()) {
    absl::string_view reporting_origin = data_body.reporting_origin();
    if (reporting_origin.empty()) {
      SCP_INFO(kFrontEndUtils, kZeroUuid, "Empty reporting origin");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    ExecutionResultOr<std::string> site =
        TransformReportingOriginToSite(std::string(reporting_origin));
    if (!site.Successful()) {
      SCP_INFO(kFrontEndUtils, kZeroUuid, "Invalid reporting origin");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (*site != authorized_domain) {
      SCP_INFO(
          kFrontEndUtils, kZeroUuid,
          absl::StrFormat(
              "The provided reporting origin does not belong to the authorized "
              "domain. reporting_origin: %s; authorized_domain: %s",
              *site, authorized_domain));
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_REPORTING_ORIGIN_NOT_BELONG_TO_SITE);
    }

    if (visited_reporting_origin.find(reporting_origin) !=
        visited_reporting_origin.end()) {
      SCP_INFO(kFrontEndUtils, kZeroUuid,
               absl::StrFormat("Repeated reporting origin found : %s",
                               reporting_origin))
      return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
    }
    visited_reporting_origin.emplace(reporting_origin);

    for (const auto& key_body : data_body.keys()) {
      ExecutionResult execution_result =
          key_body_processer(key_body, key_index, reporting_origin);
      if (!execution_result.Successful()) {
        return execution_result;
      }

      ++key_index;
    }
  }
  return SuccessExecutionResult();
}

}  // namespace privacy_sandbox::pbs
