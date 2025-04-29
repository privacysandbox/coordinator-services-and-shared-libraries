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

#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <nlohmann/json.hpp>

#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/http_types.h"
#include "cc/core/interface/type_def.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "proto/pbs/api/v1/api.pb.h"

namespace google::scp::pbs {

core::ExecutionResult ParseBeginTransactionRequestBody(
    const std::string& authorized_domain, const std::string& transaction_origin,
    const privacy_sandbox::pbs_common::BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list);

core::ExecutionResult SerializeTransactionFailedCommandIndicesResponse(
    const std::vector<size_t> command_failed_indices,
    bool should_use_request_response_protos,
    privacy_sandbox::pbs_common::BytesBuffer& response_body);

core::ExecutionResult ExtractTransactionIdFromHTTPHeaders(
    const std::shared_ptr<privacy_sandbox::pbs_common::HttpHeaders>&
        request_headers,
    privacy_sandbox::pbs_common::Uuid& uuid);

core::ExecutionResult ExtractRequestClaimedIdentity(
    const std::shared_ptr<privacy_sandbox::pbs_common::HttpHeaders>&
        request_headers,
    std::string& claimed_identity);

std::string GetReportingOriginMetricLabel();

core::ExecutionResultOr<std::string> ExtractTransactionOrigin(
    const privacy_sandbox::pbs_common::HttpHeaders& request_headers);

core::ExecutionResultOr<std::string> TransformReportingOriginToSite(
    const std::string& reporting_origin);

[[deprecated(
    "Use proto instead of JSON. JSON parsers will be removed shortly.")]]
core::ExecutionResultOr<std::string> ValidateAndGetBudgetType(
    const nlohmann::json& request_body);

// A function type used by ParseCommonV2TransactionRequestBody.
// After validating common fields like version, reporting origin, and site,
// ParseCommonV2TransactionRequestBody iterates through each key entry in the
// request JSON. For each valid key entry, it invokes this processor function,
// passing the key JSON object, its index in the overall request, the
// associated reporting origin, and the determined budget type. This allows the
// caller to implement specific logic for handling each key entry from the JSON.
using KeyBodyProcesserFunction = absl::AnyInvocable<core::ExecutionResult(
    const nlohmann::json& key_body, const size_t key_index,
    absl::string_view reporting_origin, absl::string_view budget_type)>;
[[deprecated(
    "Use proto instead of JSON. JSON parsers will be removed shortly.")]]
core::ExecutionResult ParseCommonV2TransactionRequestBody(
    absl::string_view authorized_domain, const nlohmann::json& request_body,
    KeyBodyProcesserFunction key_body_processer);

core::ExecutionResultOr<privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest::
                            PrivacyBudgetKey::BudgetType>
ValidateAndGetBudgetType(
    const privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest& request_proto);

// A function type used by ParseCommonV2TransactionRequestProto.
// After validating common fields like version, reporting origin, and site,
// ParseCommonV2TransactionRequestProto iterates through each key entry in the
// request proto. For each valid key entry, it invokes this processor function,
// passing the key proto message, its index in the overall request, and the
// associated reporting origin. This allows the caller to implement specific
// logic for handling each key entry.
using ProtoKeyBodyProcesserFunction = absl::AnyInvocable<core::ExecutionResult(
    const privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest::
        PrivacyBudgetKey& key_body,
    const size_t key_index, absl::string_view reporting_origin)>;
core::ExecutionResult ParseCommonV2TransactionRequestProto(
    absl::string_view authorized_domain,
    const privacy_sandbox::pbs::v1::ConsumePrivacyBudgetRequest& request_proto,
    ProtoKeyBodyProcesserFunction key_body_processer);

}  // namespace google::scp::pbs
