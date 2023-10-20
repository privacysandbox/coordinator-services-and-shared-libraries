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

#include "tee_aws_kms_client_provider_utils.h"

#include <string>

using std::string;

namespace {
constexpr char kDecryptResultPrefix[] = "PLAINTEXT: ";
}

namespace google::scp::cpio::client_providers {
void TeeAwsKmsClientProviderUtils::ExtractPlaintext(
    const string& decrypt_result, string& plaintext) noexcept {
  int8_t prefix_size = string(kDecryptResultPrefix).length();
  auto length = decrypt_result.length();
  auto start = decrypt_result.find(kDecryptResultPrefix);
  if (start == std::string::npos) {
    start = 0;
  } else {
    start = start + prefix_size;
    length -= start;
  }
  if (decrypt_result.back() == '\n') {
    length -= 1;
  }
  plaintext = decrypt_result.substr(start, length);
}
}  // namespace google::scp::cpio::client_providers
