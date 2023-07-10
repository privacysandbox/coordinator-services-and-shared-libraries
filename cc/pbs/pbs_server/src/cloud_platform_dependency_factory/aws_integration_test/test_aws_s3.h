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

#include "core/blob_storage_provider/src/aws/aws_s3.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/blob_storage_provider_interface.h"
#include "core/interface/config_provider_interface.h"

namespace google::scp::pbs {
/*! @copydoc AwsS3Provider
 */
class TestAwsS3Provider : public core::blob_storage_provider::AwsS3Provider {
 public:
  explicit TestAwsS3Provider(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : AwsS3Provider(async_executor, io_async_executor, config_provider) {}

 protected:
  core::ExecutionResult CreateClientConfig() noexcept override;
  void CreateS3() noexcept override;
};
}  // namespace google::scp::pbs
