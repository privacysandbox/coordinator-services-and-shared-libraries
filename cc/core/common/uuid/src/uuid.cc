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

#include "uuid.h"

#include <cctype>
#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>

#include "cc/core/common/time_provider/src/time_provider.h"

#include "error_codes.h"

using std::atomic;
using std::isxdigit;
using std::mt19937;
using std::random_device;
using std::string;
using std::uniform_int_distribution;

static constexpr char kHexMap[] = {"0123456789ABCDEF"};

namespace google::scp::core::common {
Uuid Uuid::GenerateUuid() noexcept {
  // TODO: Might want to use GetUniqueWallTimestampInNanoseconds()
  static atomic<Timestamp> current_clock(
      TimeProvider::GetWallTimestampInNanosecondsAsClockTicks());

  uint64_t high = current_clock.fetch_add(1);
  static random_device random_device_local;
  static mt19937 random_generator(random_device_local());
  uniform_int_distribution<uint64_t> distribution;

  uint64_t low = distribution(random_generator);
  return Uuid{.high = high, .low = low};
}

void AppendHex(int byte, std::string& string_to_append) {
  int first_digit = byte >> 4;
  string_to_append += kHexMap[first_digit];

  int second_digit = byte & 0x0F;
  string_to_append += kHexMap[second_digit];
}

inline int HexToPosition(char hex) {
  return std::isdigit(hex) ? (hex - '0') : (hex - 'A' + 10);
}

inline uint64_t ReadHex(const std::string& string_to_read, int offset) {
  return HexToPosition(string_to_read[offset]) * 16 +
         HexToPosition(string_to_read[offset + 1]);
}

std::string ToString(const Uuid& uuid) noexcept {
  // Uuid has two 8 bytes variable, high and low. Printing each byte to a
  // hexadecimal value a guid can be generated.
  string uuid_string;
  uuid_string.reserve(36);

  auto high = uuid.high;
  auto low = uuid.low;

  // Guid format is 00000000-0000-0000-0000-000000000000
  // 4 bytes
  AppendHex((high >> 56) & 0xFF, uuid_string);
  AppendHex((high >> 48) & 0xFF, uuid_string);
  AppendHex((high >> 40) & 0xFF, uuid_string);
  AppendHex((high >> 32) & 0xFF, uuid_string);

  uuid_string += "-";

  // 2 bytes
  AppendHex((high >> 24) & 0xFF, uuid_string);
  AppendHex((high >> 16) & 0xFF, uuid_string);

  uuid_string += "-";

  // 2 bytes
  AppendHex((high >> 8) & 0xFF, uuid_string);
  AppendHex(high & 0xFF, uuid_string);

  uuid_string += "-";

  // 2 bytes
  AppendHex((low >> 56) & 0xFF, uuid_string);
  AppendHex((low >> 48) & 0xFF, uuid_string);

  uuid_string += "-";

  // 6 bytes
  AppendHex((low >> 40) & 0xFF, uuid_string);
  AppendHex((low >> 32) & 0xFF, uuid_string);
  AppendHex((low >> 24) & 0xFF, uuid_string);
  AppendHex((low >> 16) & 0xFF, uuid_string);
  AppendHex((low >> 8) & 0xFF, uuid_string);
  AppendHex(low & 0xFF, uuid_string);

  return uuid_string;
}

ExecutionResult FromString(const std::string& uuid_string,
                           Uuid& uuid) noexcept {
  if (uuid_string.length() != 36) {
    return FailureExecutionResult(errors::SC_UUID_INVALID_STRING);
  }

  if (uuid_string[8] != '-' || uuid_string[13] != '-' ||
      uuid_string[18] != '-' || uuid_string[23] != '-') {
    return FailureExecutionResult(errors::SC_UUID_INVALID_STRING);
  }

  for (size_t i = 0; i < uuid_string.length(); ++i) {
    const char& c = uuid_string[i];
    if (c == '-') {
      continue;
    }

    if (!std::isxdigit(c)) {
      return FailureExecutionResult(errors::SC_UUID_INVALID_STRING);
    }

    if (std::islower(c)) {
      return FailureExecutionResult(errors::SC_UUID_INVALID_STRING);
    }
  }

  uuid.high =
      (ReadHex(uuid_string, 0) << 56) | (ReadHex(uuid_string, 2) << 48) |
      (ReadHex(uuid_string, 4) << 40) | (ReadHex(uuid_string, 6) << 32) |
      (ReadHex(uuid_string, 9) << 24) | (ReadHex(uuid_string, 11) << 16) |
      (ReadHex(uuid_string, 14) << 8) | (ReadHex(uuid_string, 16));

  uuid.low =
      (ReadHex(uuid_string, 19) << 56) | (ReadHex(uuid_string, 21) << 48) |
      (ReadHex(uuid_string, 24) << 40) | (ReadHex(uuid_string, 26) << 32) |
      (ReadHex(uuid_string, 28) << 24) | (ReadHex(uuid_string, 30) << 16) |
      (ReadHex(uuid_string, 32) << 8) | (ReadHex(uuid_string, 34));

  return SuccessExecutionResult();
}
}  // namespace google::scp::core::common
