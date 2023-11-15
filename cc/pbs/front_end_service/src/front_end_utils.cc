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

#include "core/common/uuid/src/uuid.h"
#include "core/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs {

namespace {

constexpr char kFrontEndUtils[] = "FrontEndUtils";
constexpr char kVersion1[] = "1.0";
constexpr char kVersion2[] = "2.0";

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
    const std::string& authorized_domain, const core::BytesBuffer& request_body,
    std::list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
    bool enable_site_based_authorization) noexcept {
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

      if (enable_site_based_authorization) {
        consume_budget_metadata.budget_key_name =
            std::make_shared<std::string>(absl::StrCat(
                authorized_domain, "/",
                consume_budget_transaction_key.at("key").get<std::string>()));
      } else {
        consume_budget_metadata.budget_key_name = std::make_shared<std::string>(
            consume_budget_transaction_key.at("key"));
      }
      consume_budget_metadata.token_count =
          consume_budget_transaction_key["token"].get<TokenCount>();

      auto reporting_time =
          consume_budget_transaction_key["reporting_time"].get<std::string>();
      if (enable_site_based_authorization) {
        auto time_bucket = ReportingTimeToTimeBucket(reporting_time);
        if (!time_bucket.Successful()) {
          return time_bucket.result();
        }
        consume_budget_metadata.time_bucket = *time_bucket;
      } else {
        google::protobuf::Timestamp reporting_timestamp;
        if (!google::protobuf::util::TimeUtil::FromString(
                reporting_time, &reporting_timestamp)) {
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

        consume_budget_metadata.time_bucket =
            reporting_time_nanoseconds.count();
      }

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
    std::list<ConsumeBudgetMetadata>& consume_budget_metadata_list) {
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

      if (visited_reporting_origin.find(reporting_origin) !=
          visited_reporting_origin.end()) {
        return core::FailureExecutionResult(
            core::errors::SC_PBS_FRONT_END_SERVICE_INVALID_REQUEST);
      }
      visited_reporting_origin.emplace(reporting_origin);

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
    std::list<ConsumeBudgetMetadata>& consume_budget_metadata_list,
    bool enable_site_based_authorization) noexcept {
  if (!enable_site_based_authorization) {
    return ParseBeginTransactionRequestBodyV1(authorized_domain, request_body,
                                              consume_budget_metadata_list,
                                              enable_site_based_authorization);
  }
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
          authorized_domain, request_body, consume_budget_metadata_list,
          enable_site_based_authorization);
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

}  // namespace google::scp::pbs
