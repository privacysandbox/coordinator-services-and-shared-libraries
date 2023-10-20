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

#include <cstddef>
#include <cstdint>
#include <string>

#include "public/core/interface/execution_result.h"

namespace google::scp::core::common {
/**
 * @brief Creates universally unique identifiers. A uuid is a 128-bit id
 * composed of two parts: high and low. The high part is a uint64_t type and
 * represents the upper 64 bits of the id. The low part is uint64_t type and
 * represents the lower 64 bits of the id.
 */
struct Uuid {
  // High value.
  uint64_t high = 0;
  uint64_t low = 0;

  bool operator==(const Uuid& other) const {
    return high == other.high && low == other.low;
  }

  bool operator!=(const Uuid& other) const { return !operator==(other); }

  bool operator<(const Uuid& other) const { return high < other.high; }

  /// Generates a random Uuid.
  static Uuid GenerateUuid() noexcept;
};

/**
 * @brief A Uuid comparator that can be used for maps and sets.
 */
struct UuidCompare {
  static size_t hash(const Uuid& uuid) { return uuid.high ^ uuid.low; }

  static bool equal(const Uuid& left, const Uuid& right) {
    return left == right;
  }
};

/**
 * @brief A Uuid hash generator that can be used for STL containers,
 * such as std::unordered_map and std::unordered_set
 */
struct UuidHash {
  size_t operator()(const Uuid& uuid) const noexcept {
    return uuid.high ^ uuid.low;
  }
};

/**
 * @brief Converts a Uuid object to string. The format of the output is a guid
 * 00000000-0000-0000-0000-000000000000.
 *
 * @param uuid The uuid to be converted to guid string.
 * @return std::string The output guid.
 */
std::string ToString(const Uuid& uuid) noexcept;

/**
 * @brief Parses a Uuid object from a provided string.
 *
 * @param uuid_string The string to parse the uuid from.
 * @param uuid The output uuid.
 * @return ExecutionResult The execution result of the operation.
 */
ExecutionResult FromString(const std::string& uuid_string, Uuid& uuid) noexcept;

static constexpr Uuid kZeroUuid{0ULL, 0ULL};
}  // namespace google::scp::core::common
