
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

#include "public/core/interface/execution_result.h"

#include <map>

#include "core/common/proto/common.pb.h"

using std::map;

namespace google::scp::core {
map<core::common::proto::ExecutionStatus, ExecutionStatus> ReverseMap(
    const map<ExecutionStatus, core::common::proto::ExecutionStatus>& m) {
  map<core::common::proto::ExecutionStatus, ExecutionStatus> r;
  for (const auto& kv : m) {
    r[kv.second] = kv.first;
  }
  return r;
}

const map<ExecutionStatus, core::common::proto::ExecutionStatus>
    kExecutionStatusToProtoMap = {
        {ExecutionStatus::Success,
         core::common::proto::ExecutionStatus::EXECUTION_STATUS_SUCCESS},
        {ExecutionStatus::Failure,
         core::common::proto::ExecutionStatus::EXECUTION_STATUS_FAILURE},
        {ExecutionStatus::Retry,
         core::common::proto::ExecutionStatus::EXECUTION_STATUS_RETRY}};

const std::map<core::common::proto::ExecutionStatus, ExecutionStatus>
    kProtoToExecutionStatusMap = ReverseMap(kExecutionStatusToProtoMap);

core::common::proto::ExecutionStatus ToStatusProto(ExecutionStatus& status) {
  return kExecutionStatusToProtoMap.at(status);
}

core::common::proto::ExecutionResult ExecutionResult::ToProto() {
  core::common::proto::ExecutionResult result_proto;
  result_proto.set_status(ToStatusProto(status));
  result_proto.set_status_code(status_code);
  return result_proto;
}

ExecutionResult::ExecutionResult(
    const core::common::proto::ExecutionResult result_proto) {
  auto mapped_status = ExecutionStatus::Failure;
  // Handle proto status UNKNOWN
  if (kProtoToExecutionStatusMap.find(result_proto.status()) !=
      kProtoToExecutionStatusMap.end()) {
    mapped_status = kProtoToExecutionStatusMap.at(result_proto.status());
  }
  status = mapped_status;
  status_code = result_proto.status_code();
}
}  // namespace google::scp::core
