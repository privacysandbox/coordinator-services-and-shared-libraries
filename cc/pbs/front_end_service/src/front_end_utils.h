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

namespace google::scp::pbs {

core::ExecutionResult ParseBeginTransactionRequestBody(
    const std::string& authorized_domain, const std::string& transaction_origin,
    const privacy_sandbox::pbs_common::BytesBuffer& request_body,
    std::vector<ConsumeBudgetMetadata>& consume_budget_metadata_list);

core::ExecutionResult SerializeTransactionFailedCommandIndicesResponse(
    const std::vector<size_t> command_failed_indices,
    privacy_sandbox::pbs_common::BytesBuffer& response_body);

core::ExecutionResult ExtractTransactionIdFromHTTPHeaders(
    const std::shared_ptr<privacy_sandbox::pbs_common::HttpHeaders>&
        request_headers,
    core::common::Uuid& uuid);

core::ExecutionResult ExtractRequestClaimedIdentity(
    const std::shared_ptr<privacy_sandbox::pbs_common::HttpHeaders>&
        request_headers,
    std::string& claimed_identity);

std::string GetReportingOriginMetricLabel(
    const std::shared_ptr<privacy_sandbox::pbs_common::HttpRequest>& request,
    const std::string& remote_coordinator_claimed_identity);

core::ExecutionResultOr<std::string> ExtractTransactionOrigin(
    const privacy_sandbox::pbs_common::HttpHeaders& request_headers);

core::ExecutionResultOr<std::string> TransformReportingOriginToSite(
    const std::string& reporting_origin);

core::ExecutionResultOr<std::string> ValidateAndGetBudgetType(
    const nlohmann::json& request_body);

using KeyBodyProcesserFunction = absl::AnyInvocable<core::ExecutionResult(
    const nlohmann::json& key_body, const size_t key_index,
    absl::string_view reporting_origin, absl::string_view budget_type)>;
core::ExecutionResult ParseCommonV2TransactionRequestBody(
    absl::string_view authorized_domain, const nlohmann::json& request_body,
    KeyBodyProcesserFunction key_body_processer);

}  // namespace google::scp::pbs
