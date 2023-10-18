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

#include "component_lifecycle_lease_event_sink.h"

#include "core/common/global_logger/src/global_logger.h"

using google::scp::core::ExecutionResult;
using google::scp::core::LeasableLockId;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseTransitionType;
using google::scp::core::ServiceInterface;
using google::scp::core::common::Uuid;
using std::optional;

static constexpr char kComponentLifecycleLeaseEventSink[] =
    "ComponentLifecycleLeaseEventSink";

namespace google::scp::core {
ComponentLifecycleLeaseEventSink::ComponentLifecycleLeaseEventSink(
    const std::shared_ptr<ServiceInterface>& component)
    : component_(component), activity_id_(Uuid::GenerateUuid()) {}

void ComponentLifecycleLeaseEventSink::OnLeaseTransition(
    const LeasableLockId& lock_id, LeaseTransitionType lease_transition_type,
    optional<LeaseInfo>) noexcept {
  ExecutionResult execution_result;
  switch (lease_transition_type) {
    case LeaseTransitionType::kAcquired:
      SCP_INFO(kComponentLifecycleLeaseEventSink, activity_id_,
               "Acquired Lease on Lock: '%s'. Starting Component",
               ToString(lock_id).c_str());
      // Upon acquring lease, start running the component.
      execution_result = component_->Run();
      if (!execution_result.Successful()) {
        SCP_ERROR(kComponentLifecycleLeaseEventSink, activity_id_,
                  execution_result, "Failed to Run() Component for Lock: '%s'",
                  ToString(lock_id).c_str());
      }
      break;
    case LeaseTransitionType::kLost:
    case LeaseTransitionType::kReleased:
      SCP_INFO(kComponentLifecycleLeaseEventSink, activity_id_,
               "Lost Lease on Lock: '%s'. Stopping Component",
               ToString(lock_id).c_str());
      // Upon losing lease, stop running the component.
      execution_result = component_->Stop();
      if (!execution_result.Successful()) {
        SCP_ERROR(kComponentLifecycleLeaseEventSink, activity_id_,
                  execution_result, "Failed to Stop() Component for Lock: '%s'",
                  ToString(lock_id).c_str());
      }
      break;
    default:
      // Ignore.
      break;
  }
}
}  // namespace google::scp::core
