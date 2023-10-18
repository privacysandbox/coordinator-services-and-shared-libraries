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

#include "core/interface/lease_manager_interface.h"

namespace google::scp::core {
/**
 * @brief ComponentLifecycleLeaseEventSink. This reacts to the lease events on a
 * lock. This simply starts (Run()) the component supplied upon acquiring lease
 * and stops (Stop()) the component upon losing the lease.
 */
class ComponentLifecycleLeaseEventSink : public core::LeaseEventSinkInterface {
 public:
  ComponentLifecycleLeaseEventSink(
      const std::shared_ptr<core::ServiceInterface>& component);

  void OnLeaseTransition(const core::LeasableLockId&, core::LeaseTransitionType,
                         std::optional<core::LeaseInfo>) noexcept override;

 protected:
  /// @brief component to start/stop
  std::shared_ptr<core::ServiceInterface> component_;
  /// @brief activity ID
  core::common::Uuid activity_id_;
};
}  // namespace google::scp::core
