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

#include "hashing.h"

#include <memory>
#include <string>

#include <openssl/md5.h>

#include "cc/core/interface/type_def.h"

#include "error_codes.h"

namespace google::scp::core::utils {

using ::privacy_sandbox::pbs_common::BytesBuffer;
using std::make_unique;
using std::string;

ExecutionResultOr<string> CalculateMd5Hash(const BytesBuffer& buffer) {
  if (buffer.length == 0) {
    return FailureExecutionResult(
        privacy_sandbox::pbs_common::SC_CORE_UTILS_INVALID_INPUT);
  }

  unsigned char digest_length[MD5_DIGEST_LENGTH];
  MD5_CTX md5_context;
  MD5_Init(&md5_context);
  MD5_Update(&md5_context, buffer.bytes->data(), buffer.length);

  MD5_Final(digest_length, &md5_context);
  return string(reinterpret_cast<char*>(digest_length), MD5_DIGEST_LENGTH);
}

ExecutionResultOr<string> CalculateMd5Hash(const string& buffer) {
  if (buffer.length() == 0) {
    return FailureExecutionResult(
        privacy_sandbox::pbs_common::SC_CORE_UTILS_INVALID_INPUT);
  }

  unsigned char digest_length[MD5_DIGEST_LENGTH];
  MD5_CTX md5_context;
  MD5_Init(&md5_context);
  MD5_Update(&md5_context, buffer.c_str(), buffer.length());

  MD5_Final(digest_length, &md5_context);

  return string(reinterpret_cast<char*>(digest_length), MD5_DIGEST_LENGTH);
}

ExecutionResult CalculateMd5Hash(const BytesBuffer& buffer, string& checksum) {
  ASSIGN_OR_RETURN(checksum, CalculateMd5Hash(buffer));
  return SuccessExecutionResult();
}

ExecutionResult CalculateMd5Hash(const string& buffer, string& checksum) {
  ASSIGN_OR_RETURN(checksum, CalculateMd5Hash(buffer));
  return SuccessExecutionResult();
}

}  // namespace google::scp::core::utils
