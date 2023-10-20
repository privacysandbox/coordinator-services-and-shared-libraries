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

#include "test_gcp_instance_client_provider.h"

#include <map>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "public/core/interface/execution_result.h"

using absl::StrFormat;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {
constexpr char kGcpResourceNameFormat[] =
    R"(//compute.googleapis.com/projects/%s/zones/%s/instances/%s)";
}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult
TestGcpInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  resource_name = StrFormat(kGcpResourceNameFormat, test_options_->owner_id,
                            test_options_->zone, test_options_->instance_id);
  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio::client_providers
