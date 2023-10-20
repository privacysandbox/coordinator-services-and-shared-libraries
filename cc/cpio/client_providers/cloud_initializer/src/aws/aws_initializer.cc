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

#include "aws_initializer.h"

#include <memory>

#include <aws/core/Aws.h>

#include "public/core/interface/execution_result.h"

using Aws::InitAPI;
using Aws::ShutdownAPI;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::make_shared;

namespace google::scp::cpio::client_providers {
ExecutionResult AwsInitializer::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsInitializer::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsInitializer::Stop() noexcept {
  return SuccessExecutionResult();
}

void AwsInitializer::InitCloud() noexcept {
  InitAPI(options_);
}

void AwsInitializer::ShutdownCloud() noexcept {
  ShutdownAPI(options_);
}

std::shared_ptr<CloudInitializerInterface> CloudInitializerFactory::Create() {
  return make_shared<AwsInitializer>();
}
}  // namespace google::scp::cpio::client_providers
