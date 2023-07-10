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

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/message_router/src/message_router.h"
#include "cpio/client_providers/metric_client_provider/mock/mock_metric_client_provider.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::mock {
class MockMetricClientWithOverrides : public MetricClient {
 public:
  MockMetricClientWithOverrides(
      const std::shared_ptr<MetricClientOptions>& options)
      : MetricClient(options) {
    metric_client_provider_ =
        std::make_shared<client_providers::mock::MockMetricClientProvider>();
  }

  std::shared_ptr<client_providers::mock::MockMetricClientProvider>
  GetMetricClientProvider() {
    return std::dynamic_pointer_cast<
        client_providers::mock::MockMetricClientProvider>(
        metric_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock
