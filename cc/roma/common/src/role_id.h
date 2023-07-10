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

#include <stdint.h>

namespace google::scp::roma::common {
// RoleId is an identifier of the process/thread roles. It has a numeric
// representation that indicates the worker index. The highest bit indicates if
// the current process/thread is dispatcher.
class RoleId {
 public:
  explicit RoleId(uint32_t index, bool is_dispatcher = true) : index_(0) {
    SetId(index, is_dispatcher);
  }

  /// Default constructor.
  RoleId() : index_(kBadRoleMask) {}

  /// Get the worker index of this RoleId.
  uint32_t GetId() const { return index_ & ~kDispatcherMask; }

  /// Set the RoleId value by specifying index and is_dispatcher.
  void SetId(uint32_t index, bool is_dispatcher = false) {
    uint32_t hi_bit = static_cast<uint32_t>(is_dispatcher);
    hi_bit <<= kDispatcherBit;
    index_ = index | hi_bit;
  }

  /// Returns true if the role is a dispatcher role.
  bool IsDispatcher() const { return index_ & kDispatcherMask; }

  /// If the role is bad or uninitialized.
  bool Bad() const { return (index_ & kBadRoleMask) == kBadRoleMask; }

 private:
  static constexpr uint32_t kDispatcherBit = 31U;
  static constexpr uint32_t kDispatcherMask = 1U << kDispatcherBit;
  // All 1s in lower 31 bits indicate bad role (0x7FFFFFFF or 0xFFFFFFFF).
  static constexpr uint32_t kBadRoleMask = (1U << kDispatcherBit) - 1;
  uint32_t index_;
};
}  // namespace google::scp::roma::common
