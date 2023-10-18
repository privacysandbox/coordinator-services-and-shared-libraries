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

#include <gmock/gmock.h>

#include <memory>
#include <optional>
#include <string>

#include "core/interface/lease_manager_interface.h"

namespace google::scp::core::lease_manager::mock {
class MockLeaseStatistics : public LeaseStatisticsInterface {
 public:
  MOCK_METHOD(size_t, GetCurrentlyLeasedLocksCount, (), (noexcept, override));
};
}  // namespace google::scp::core::lease_manager::mock
