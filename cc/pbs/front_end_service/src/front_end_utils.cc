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

#include "front_end_utils.h"

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
using ::privacy_sandbox::pbs_common::HttpRequest;
using ::privacy_sandbox::pbs_common::kClaimedIdentityHeader;
using ::privacy_sandbox::pbs_common::kZeroUuid;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using ::privacy_sandbox::pbs_common::Uuid;

constexpr absl::string_view kFrontEndUtils = "FrontEndUtils";
constexpr absl::string_view kVersion1 = "1.0";
constexpr absl::string_view kVersion2 = "2.0";
constexpr absl::string_view kHttpPrefix = "http://";
constexpr absl::string_view kHttpsPrefix = "https://";
constexpr absl::string_view kBudgetTypeBinaryBudget =
    "BUDGET_TYPE_BINARY_BUDGET";

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

// V1 Request Example:
// {
//   v: "1.0",
//   t: [
//     {
//       "key": "<string>",
//       "token": <uint8_t>,
//       "reporting_time": "<string>"
//     },
//     ....
//   ]
// }
ExecutionResult ParseBeginTransactionRequestBodyV1(
    const std::string& transaction_origin, const BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) {
  try {
    auto transaction_request = nlohmann::json::parse(
        request_body.bytes->begin(), request_body.bytes->end());

    /// The body format of the begin transaction request is:
    /// {v: "1.0", t: [{ key: '', token: '', reporting_time: ''}, ....]}
    if (transaction_request.find("v") == transaction_request.end() ||
        transaction_request.find("t") == transaction_request.end()) {
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (transaction_request["v"] != "1.0") {
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    std::unordered_set<std::string> visited;
    for (auto it = transaction_request["t"].begin();
         it != transaction_request["t"].end(); ++it) {
      auto consume_budget_transaction_key = it.value();
      ConsumeBudgetMetadata consume_budget_metadata;

      if (consume_budget_transaction_key.count("key") == 0 ||
          consume_budget_transaction_key.count("token") == 0 ||
          consume_budget_transaction_key.count("reporting_time") == 0) {
        return FailureExecutionResult(
            SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
      }

      consume_budget_metadata.budget_key_name =
          std::make_shared<std::string>(absl::StrCat(
              transaction_origin, "/",
              consume_budget_transaction_key.at("key").get<std::string>()));
      consume_budget_metadata.token_count =
          consume_budget_transaction_key["token"].get<TokenCount>();

      auto reporting_time =
          consume_budget_transaction_key["reporting_time"].get<std::string>();
      auto time_bucket_or = ReportingTimeToTimeBucket(reporting_time);
      if (!time_bucket_or.Successful()) {
        return time_bucket_or.result();
      }
      consume_budget_metadata.time_bucket = *time_bucket_or;

      // TODO: This is a temporary solution to prevent transaction
      // commands belong to the same reporting hour to execute within the
      // same transaction. The proper solution is to move this logic to
      // the transaction commands.
      auto time_group =
          Utils::GetTimeGroup(consume_budget_metadata.time_bucket);
      auto time_bucket =
          Utils::GetTimeBucket(consume_budget_metadata.time_bucket);

      auto visited_str = *consume_budget_metadata.budget_key_name + "_" +
                         std::to_string(time_group) + "_" +
                         std::to_string(time_bucket);

      if (visited.find(visited_str) != visited.end()) {
        return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
      }

      visited.emplace(visited_str);
      consume_budget_metadata_list.push_back(consume_budget_metadata);
    }
  }

  catch (...) {
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  return SuccessExecutionResult();
}

// V2 Request Example:
// {
//   "v": "2.0",
//   "data": [
//     {
//       "reporting_origin": <reporting_origin_string>,
//       "keys": [{
//         "key": "<string>",
//         "token": <uint8_t>,
//         "reporting_time": "<string>"
//       }]
//     }
//   ]
// }
ExecutionResult ParseBeginTransactionRequestBodyV2(
    const nlohmann::json& transaction_request,
    const std::string& authorized_domain,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) {
  auto transaction_request_data_it = transaction_request.find("data");
  if (transaction_request_data_it == transaction_request.end()) {
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  consume_budget_metadata_list.clear();
  size_t consume_budget_metadata_list_size = 0;
  for (auto it = transaction_request_data_it->begin();
       it != transaction_request_data_it->end(); ++it) {
    auto keys_it = it->find("keys");
    if (keys_it == it->end()) {
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    for (auto key_it = keys_it->begin(); key_it != keys_it->end(); ++key_it) {
      ++consume_budget_metadata_list_size;
    }
  }

  if (consume_budget_metadata_list_size == 0) {
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  consume_budget_metadata_list.reserve(consume_budget_metadata_list_size);

  absl::flat_hash_set<std::string> visited;
  absl::flat_hash_set<std::string> visited_reporting_origin;
  for (auto it = transaction_request_data_it->begin();
       it != transaction_request_data_it->end(); ++it) {
    auto reporting_origin_it = it->find("reporting_origin");
    auto keys_it = it->find("keys");

    if (reporting_origin_it == it->end() || keys_it == it->end()) {
      consume_budget_metadata_list.clear();
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    const std::string& reporting_origin =
        reporting_origin_it->get<std::string>();
    if (reporting_origin.empty()) {
      consume_budget_metadata_list.clear();
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    ExecutionResultOr<std::string> site =
        TransformReportingOriginToSite(reporting_origin);
    if (!site.Successful()) {
      consume_budget_metadata_list.clear();
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
      consume_budget_metadata_list.clear();
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_REPORTING_ORIGIN_NOT_BELONG_TO_SITE);
    }

    if (visited_reporting_origin.find(reporting_origin) !=
        visited_reporting_origin.end()) {
      return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
    }
    visited_reporting_origin.emplace(reporting_origin);

    for (auto key_it = keys_it->begin(); key_it != keys_it->end(); ++key_it) {
      auto k_it = key_it->find("key");
      auto token_it = key_it->find("token");
      auto reporting_time_it = key_it->find("reporting_time");

      if (k_it == key_it->end() || token_it == key_it->end() ||
          reporting_time_it == key_it->end()) {
        consume_budget_metadata_list.clear();
        return FailureExecutionResult(
            SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
      }

      auto budget_key_name = std::make_shared<std::string>(
          absl::StrCat(reporting_origin, "/", k_it->get<std::string>()));
      TokenCount token_count = token_it->get<TokenCount>();
      const std::string& reporting_time = reporting_time_it->get<std::string>();
      auto reporting_timestamp = ReportingTimeToTimeBucket(reporting_time);
      if (!reporting_timestamp.Successful()) {
        return reporting_timestamp.result();
      }

      TimeGroup time_group = Utils::GetTimeGroup(*reporting_timestamp);
      TimeBucket time_bucket = Utils::GetTimeBucket(*reporting_timestamp);
      std::string visited_key =
          absl::StrCat(*budget_key_name, "_", time_group, "_", time_bucket);
      if (visited.find(visited_key) != visited.end()) {
        return FailureExecutionResult(SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
      }
      visited.emplace(visited_key);

      consume_budget_metadata_list.emplace_back(ConsumeBudgetMetadata{
          budget_key_name, token_count, *reporting_timestamp});
    }
  }

  return SuccessExecutionResult();
}

}  // namespace

ExecutionResult SerializeTransactionFailedCommandIndicesResponse(
    const std::vector<size_t> command_failed_indices,
    bool should_use_request_response_protos, BytesBuffer& response_body) {
  std::string serialized;
  if (should_use_request_response_protos) {
    ConsumePrivacyBudgetResponse response_proto;
    response_proto.set_version(kVersion1);
    response_proto.mutable_exhausted_budget_indices()->Assign(
        command_failed_indices.begin(), command_failed_indices.end());

    JsonPrintOptions json_print_options;
    json_print_options.always_print_fields_with_no_presence = true;

    if (auto status = MessageToJsonString(response_proto, &serialized,
                                          json_print_options);
        !status.ok()) {
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY);
    }
  } else {
    try {
      nlohmann::json serialized_body =
          nlohmann::json::parse("{\"v\": \"1.0\"}");
      serialized_body["f"] = nlohmann::json::parse("[]");
      for (auto index : command_failed_indices) {
        serialized_body["f"].push_back(index);
      }
      serialized = serialized_body.dump();
    } catch (...) {
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_RESPONSE_BODY);
    }
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

[[deprecated("No longer needed when budget consumer is enabled in PBS.")]]
ExecutionResult ParseBeginTransactionRequestBody(
    const std::string& authorized_domain, const std::string& transaction_origin,
    const BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) {
  try {
    nlohmann::json transaction_request = nlohmann::json::parse(
        request_body.bytes->begin(), request_body.bytes->end(),
        /*parser_callback_t=*/nullptr,
        /*allow_exceptions=*/false);
    if (transaction_request.is_discarded()) {
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    if (transaction_request.find("v") == transaction_request.end()) {
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    if (transaction_request["v"] != kVersion1 &&
        transaction_request["v"] != kVersion2) {
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (transaction_request["v"] == kVersion1) {
      return ParseBeginTransactionRequestBodyV1(
          transaction_origin, request_body, consume_budget_metadata_list);
    }

    // transaction_request["v"] == "2.0"
    return ParseBeginTransactionRequestBodyV2(
        transaction_request, authorized_domain, consume_budget_metadata_list);
  } catch (const std::exception& exception) {
    SCP_INFO(kFrontEndUtils, kZeroUuid,
             absl::StrCat("ParseBeginTransactionRequestBody failed ",
                          exception.what()));
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }
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

ExecutionResultOr<std::string> ValidateAndGetBudgetType(
    const nlohmann::json& request_body) {
  try {
    auto version_it = request_body.find("v");
    if (version_it == request_body.end()) {
      SCP_INFO(kFrontEndUtils, kZeroUuid, "JSON key absent : \"v\"");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    if (version_it->get<std::string>() == kVersion1) {
      // Version 1 is only supported in binary budget consumer
      return std::string(kBudgetTypeBinaryBudget);
    }

    auto request_body_data_it = request_body.find("data");
    if (request_body_data_it == request_body.end()) {
      SCP_INFO(kFrontEndUtils, kZeroUuid, "JSON key absent : \"data\"");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (request_body_data_it->empty()) {
      // Default is binary budget consumer
      return std::string(kBudgetTypeBinaryBudget);
    }

    std::string budget_type = "";
    for (const auto& data_body : *request_body_data_it) {
      auto keys_it = data_body.find("keys");
      if (keys_it == data_body.end()) {
        SCP_INFO(kFrontEndUtils, kZeroUuid, "JSON keys absent :  \"keys\"");
        return FailureExecutionResult(
            SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
      }

      if (keys_it->empty()) {
        // Default is binary budget consumer
        return std::string(kBudgetTypeBinaryBudget);
      }

      for (const auto& key_body : *keys_it) {
        auto budget_type_it = key_body.find("budget_type");
        std::string key_budget_type = std::string(kBudgetTypeBinaryBudget);
        if (budget_type_it != key_body.end()) {
          key_budget_type = budget_type_it->get<std::string>();
          if (key_budget_type.empty()) {
            SCP_INFO(kFrontEndUtils, kZeroUuid, "Empty budget type");
            return FailureExecutionResult(
                SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
          }
        }

        // All keys should have the same budget type.
        if (!budget_type.empty() && key_budget_type != budget_type) {
          SCP_INFO(kFrontEndUtils, kZeroUuid,
                   absl::StrFormat("All keys should have the same budget type. "
                                   "Expected %s Found %s",
                                   budget_type, key_budget_type));
          return FailureExecutionResult(
              SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
        }
        budget_type = key_budget_type;
      }
    }

    return budget_type;
  } catch (const std::exception& exception) {
    // We can expect exceptions from failing types. For example if "data" is set
    // to something other than an array.
    SCP_INFO(
        kFrontEndUtils, kZeroUuid,
        absl::StrCat("ValidateAndGetBudgetType failed ", exception.what()));
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }
}

ExecutionResult ParseCommonV2TransactionRequestBody(
    absl::string_view authorized_domain, const nlohmann::json& request_body,
    KeyBodyProcesserFunction key_body_processer) {
  auto version_it = request_body.find("v");
  if (version_it == request_body.end()) {
    SCP_INFO(kFrontEndUtils, kZeroUuid, "JSON key absent : \"v\"");
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  if (version_it->get<std::string>() != kVersion2) {
    SCP_INFO(kFrontEndUtils, kZeroUuid, "Not a version 2.0 request");
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  auto request_body_data_it = request_body.find("data");
  if (request_body_data_it == request_body.end()) {
    SCP_INFO(kFrontEndUtils, kZeroUuid, "JSON key absent : \"data\"");
    return FailureExecutionResult(
        SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  absl::flat_hash_set<std::string> visited_reporting_origin;
  size_t key_index = 0;

  for (const auto& data_body : *request_body_data_it) {
    auto reporting_origin_it = data_body.find("reporting_origin");
    auto keys_it = data_body.find("keys");

    if (reporting_origin_it == data_body.end() || keys_it == data_body.end()) {
      SCP_INFO(kFrontEndUtils, kZeroUuid,
               "One of more JSON keys absent : "
               "\"reporting_origin\" or \"keys\"");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    const std::string& reporting_origin =
        reporting_origin_it->get<std::string>();
    if (reporting_origin.empty()) {
      SCP_INFO(kFrontEndUtils, kZeroUuid, "Empty reporting origin");
      return FailureExecutionResult(
          SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    ExecutionResultOr<std::string> site =
        TransformReportingOriginToSite(reporting_origin);
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

    for (const auto& key_body : *keys_it) {
      auto budget_type_it = key_body.find("budget_type");
      std::string key_budget_type = std::string(kBudgetTypeBinaryBudget);
      if (budget_type_it != key_body.end()) {
        key_budget_type = budget_type_it->get<std::string>();
        if (key_budget_type.empty()) {
          SCP_INFO(kFrontEndUtils, kZeroUuid, "Empty budget type");
          return FailureExecutionResult(
              SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
        }
      }

      ExecutionResult execution_result = key_body_processer(
          key_body, key_index, reporting_origin, key_budget_type);
      if (!execution_result.Successful()) {
        return execution_result;
      }

      ++key_index;
    }
  }
  return SuccessExecutionResult();
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
