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

#include "base64.h"

#include <memory>
#include <string>

#include <openssl/base64.h>

#include "error_codes.h"

using std::make_unique;
using std::string;

namespace google::scp::core::utils {
ExecutionResult Base64Decode(const string& encoded, string& decoded) {
  if ((encoded.length() % 4) != 0) {
    return FailureExecutionResult(
        errors::SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH);
  }
  size_t required_len = 0;
  if (EVP_DecodedLength(&required_len, encoded.length()) == 0) {
    return FailureExecutionResult(errors::SC_CORE_UTILS_INVALID_INPUT);
  }
  auto buffer = make_unique<uint8_t[]>(required_len);

  size_t output_len = 0;
  int ret = EVP_DecodeBase64(buffer.get(), &output_len, required_len,
                             reinterpret_cast<const uint8_t*>(encoded.data()),
                             encoded.length());
  if (ret == 0) {
    return FailureExecutionResult(errors::SC_CORE_UTILS_INVALID_INPUT);
  }
  decoded = string(reinterpret_cast<char*>(buffer.get()), output_len);
  return SuccessExecutionResult();
}

ExecutionResult Base64Encode(const string& decoded, string& encoded) {
  size_t required_len = 0;
  if (EVP_EncodedLength(&required_len, decoded.length()) == 0) {
    return FailureExecutionResult(errors::SC_CORE_UTILS_INVALID_INPUT);
  }
  auto buffer = make_unique<uint8_t[]>(required_len);

  int ret = EVP_EncodeBlock(buffer.get(),
                            reinterpret_cast<const uint8_t*>(decoded.data()),
                            decoded.length());
  if (ret == 0) {
    return FailureExecutionResult(errors::SC_CORE_UTILS_INVALID_INPUT);
  }
  encoded = string(reinterpret_cast<char*>(buffer.get()), ret);
  return SuccessExecutionResult();
}

ExecutionResultOr<string> PadBase64Encoding(const string& encoded) {
  ExecutionResultOr<string> ret_val;
  switch (encoded.length() % 4) {
    case 0:
      ret_val.emplace<string>(encoded);
      break;
    case 2:
      ret_val.emplace<string>(encoded + "==");
      break;
    case 3:
      ret_val.emplace<string>(encoded + "=");
      break;
    case 1:
    default:
      // Base64 encoded representation consists of 4 (6-bit) characters, to
      // represent 3 (8-bit) decoded characters. A single encoded character is
      // 6-bit and is not enough in size to represent a decoded character of 8
      // bits.
      ret_val = FailureExecutionResult(
          errors::SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH);
      break;
  }
  return ret_val;
}

}  // namespace google::scp::core::utils
