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

#include <functional>
#include <memory>

#include <aws/core/utils/threading/Executor.h>

#include "core/interface/async_executor_interface.h"

namespace google::scp::core::async_executor::aws {
/**
 * @brief Custom async executor the AWS library. The current executor
 * implemented in the AWS SDK uses a singled queue with locks which does not
 * provide high throughput as needed. This is a wrapper around the async
 * executor to redirect all the requests to the SCP AsyncExecutor.
 */
class AwsAsyncExecutor : public Aws::Utils::Threading::Executor {
 public:
  AwsAsyncExecutor(
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor)
      : async_executor_(async_executor) {}

 protected:
  bool SubmitToThread(std::function<void()>&& task) override {
    if (async_executor_->Schedule([task]() { task(); },
                                  core::AsyncPriority::Normal) ==
        SuccessExecutionResult()) {
      return true;
    }

    return false;
  }

 private:
  const std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
};
}  // namespace google::scp::core::async_executor::aws
