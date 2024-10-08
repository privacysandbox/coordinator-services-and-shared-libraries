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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/util/time_util.h>
#include <nlohmann/json.hpp>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"
#include "cc/core/common/global_logger/src/global_logger.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/type_def.h"
#include "cc/pbs/budget_key_timeframe_manager/src/budget_key_timeframe_utils.h"
#include "cc/pbs/front_end_service/src/error_codes.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::scp::pbs {

namespace {

using ::google::scp::core::ExecutionResultOr;

constexpr char kFrontEndUtils[] = "FrontEndUtils";
constexpr char kVersion1[] = "1.0";
constexpr char kVersion2[] = "2.0";
constexpr char kHttpPrefix[] = "http://";
constexpr char kHttpsPrefix[] = "https://";

core::ExecutionResultOr<TimeBucket> ReportingTimeToTimeBucket(
    const std::string& reporting_time) {
  google::protobuf::Timestamp reporting_timestamp;
  if (!google::protobuf::util::TimeUtil::FromString(reporting_time,
                                                    &reporting_timestamp)) {
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
  }

  if (reporting_timestamp.seconds() < 0) {
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
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
core::ExecutionResult ParseBeginTransactionRequestBodyV1(
    const std::string& transaction_origin,
    const core::BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) noexcept {
  try {
    auto transaction_request = nlohmann::json::parse(
        request_body.bytes->begin(), request_body.bytes->end());

    /// The body format of the begin transaction request is:
    /// {v: "1.0", t: [{ key: '', token: '', reporting_time: ''}, ....]}
    if (transaction_request.find("v") == transaction_request.end() ||
        transaction_request.find("t") == transaction_request.end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (transaction_request["v"] != "1.0") {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    std::unordered_set<std::string> visited;
    for (auto it = transaction_request["t"].begin();
         it != transaction_request["t"].end(); ++it) {
      auto consume_budget_transaction_key = it.value();
      ConsumeBudgetMetadata consume_budget_metadata;

      if (consume_budget_transaction_key.count("key") == 0 ||
          consume_budget_transaction_key.count("token") == 0 ||
          consume_budget_transaction_key.count("reporting_time") == 0) {
        return core::FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
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
      auto time_group = budget_key_timeframe_manager::Utils::GetTimeGroup(
          consume_budget_metadata.time_bucket);
      auto time_bucket = budget_key_timeframe_manager::Utils::GetTimeBucket(
          consume_budget_metadata.time_bucket);

      auto visited_str = *consume_budget_metadata.budget_key_name + "_" +
                         std::to_string(time_group) + "_" +
                         std::to_string(time_bucket);

      if (visited.find(visited_str) != visited.end()) {
        return core::FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
      }

      visited.emplace(visited_str);
      consume_budget_metadata_list.push_back(consume_budget_metadata);
    }
  }

  catch (...) {
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  return core::SuccessExecutionResult();
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
core::ExecutionResult ParseBeginTransactionRequestBodyV2(
    const nlohmann::json& transaction_request,
    const std::string& authorized_domain,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) {
  if (!transaction_request.contains("data")) {
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> visited_reporting_origin;
  for (auto it = transaction_request["data"].begin();
       it != transaction_request["data"].end(); ++it) {
    if (!it->contains("reporting_origin") || !it->contains("keys")) {
      consume_budget_metadata_list.clear();
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    std::string reporting_origin =
        it->at("reporting_origin").get<std::string>();
    if (reporting_origin.empty()) {
      consume_budget_metadata_list.clear();
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    ExecutionResultOr<std::string> site =
        TransformReportingOriginToSite(reporting_origin);
    if (!site.Successful()) {
      consume_budget_metadata_list.clear();
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (*site != authorized_domain) {
      SCP_INFO(
          kFrontEndUtils, core::common::kZeroUuid,
          absl::StrFormat(
              "The provided reporting origin does not belong to the authorized "
              "domain. reporting_origin: %s; authorized_domain: %s",
              *site, authorized_domain));
      consume_budget_metadata_list.clear();
      return core::FailureExecutionResult(
          core::errors::
              SC_PBS_FRONT_END_SERVICE_REPORTING_ORIGIN_NOT_BELONG_TO_SITE);
    }

    if (visited_reporting_origin.find(reporting_origin) !=
        visited_reporting_origin.end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
    }
    visited_reporting_origin.emplace(reporting_origin);

    for (auto key_it = it->at("keys").begin(); key_it != it->at("keys").end();
         ++key_it) {
      if (!key_it->contains("key") || !key_it->contains("token") ||
          !key_it->contains("reporting_time")) {
        consume_budget_metadata_list.clear();
        return core::FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
      }

      auto budget_key_name = std::make_shared<std::string>(absl::StrCat(
          reporting_origin, "/", key_it->at("key").get<std::string>()));
      TokenCount token_count = key_it->at("token").get<TokenCount>();
      std::string reporting_time =
          key_it->at("reporting_time").get<std::string>();
      auto reporting_timestamp = ReportingTimeToTimeBucket(reporting_time);
      if (!reporting_timestamp.Successful()) {
        return reporting_timestamp.result();
      }

      // TODO: This is a temporary solution to prevent transaction
      // commands belong to the same reporting hour to execute within the
      // same transaction. The proper solution is to move this logic to
      // the transaction commands.
      TimeGroup time_group = budget_key_timeframe_manager::Utils::GetTimeGroup(
          *reporting_timestamp);
      TimeBucket time_bucket =
          budget_key_timeframe_manager::Utils::GetTimeBucket(
              *reporting_timestamp);
      std::string visited_key =
          absl::StrCat(*budget_key_name, "_", std::to_string(time_group) + "_",
                       std::to_string(time_bucket));
      if (visited.find(visited_key) != visited.end()) {
        return core::FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
      }
      visited.emplace(visited_key);

      consume_budget_metadata_list.emplace_back(ConsumeBudgetMetadata{
          budget_key_name, token_count, *reporting_timestamp});
    }
  }

  if (consume_budget_metadata_list.empty()) {
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }

  return core::SuccessExecutionResult();
}

}  // namespace

core::ExecutionResult ParseBeginTransactionRequestBody(
    const std::string& authorized_domain, const core::BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) noexcept {
  try {
    nlohmann::json transaction_request = nlohmann::json::parse(
        request_body.bytes->begin(), request_body.bytes->end(),
        /*parser_callback_t=*/nullptr,
        /*allow_exceptions=*/false);
    if (transaction_request.is_discarded()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    if (transaction_request.find("v") == transaction_request.end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    if (transaction_request["v"] != kVersion1 &&
        transaction_request["v"] != kVersion2) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (transaction_request["v"] == kVersion1) {
      return ParseBeginTransactionRequestBodyV1(authorized_domain, request_body,
                                                consume_budget_metadata_list);
    }

    // transaction_request["v"] == "2.0"
    return ParseBeginTransactionRequestBodyV2(
        transaction_request, authorized_domain, consume_budget_metadata_list);
  } catch (const std::exception& exception) {
    SCP_INFO(kFrontEndUtils, core::common::kZeroUuid,
             absl::StrCat("ParseBeginTransactionRequestBody failed ",
                          exception.what()));
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }
}

core::ExecutionResult ParseBeginTransactionRequestBody(
    const std::string& authorized_domain, const std::string& transaction_origin,
    const core::BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list) noexcept {
  try {
    nlohmann::json transaction_request = nlohmann::json::parse(
        request_body.bytes->begin(), request_body.bytes->end(),
        /*parser_callback_t=*/nullptr,
        /*allow_exceptions=*/false);
    if (transaction_request.is_discarded()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    if (transaction_request.find("v") == transaction_request.end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }
    if (transaction_request["v"] != kVersion1 &&
        transaction_request["v"] != kVersion2) {
      return core::FailureExecutionResult(
          core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
    }

    if (transaction_request["v"] == kVersion1) {
      return ParseBeginTransactionRequestBodyV1(
          transaction_origin, request_body, consume_budget_metadata_list);
    }

    // transaction_request["v"] == "2.0"
    return ParseBeginTransactionRequestBodyV2(
        transaction_request, authorized_domain, consume_budget_metadata_list);
  } catch (const std::exception& exception) {
    SCP_INFO(kFrontEndUtils, core::common::kZeroUuid,
             absl::StrCat("ParseBeginTransactionRequestBody failed ",
                          exception.what()));
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST_BODY);
  }
}

core::ExecutionResultOr<std::string> TransformReportingOriginToSite(
    const std::string& reporting_origin) {
  const psl_ctx_t* psl = psl_builtin();
  const char* private_suffix_part_start =
      psl_registrable_domain(psl, reporting_origin.c_str());
  if (private_suffix_part_start == nullptr) {
    return core::FailureExecutionResult(
        core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REPORTING_ORIGIN);
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

}  // namespace google::scp::pbs
