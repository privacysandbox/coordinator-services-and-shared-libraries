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

#include "public/core/interface/execution_result.h"
#include "public/cpio/adapters/config_client/src/config_client.h"
#include "public/cpio/test/config_client/test_aws_config_client_options.h"

namespace google::scp::cpio {
/*! @copydoc ConfigClientInterface
 */
class TestAwsConfigClient : public ConfigClient {
 public:
  explicit TestAwsConfigClient(
      const std::shared_ptr<TestAwsConfigClientOptions>& options)
      : ConfigClient(options) {}
};
}  // namespace google::scp::cpio
