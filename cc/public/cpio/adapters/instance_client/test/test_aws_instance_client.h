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
#include "public/cpio/adapters/instance_client/src/instance_client.h"
#include "public/cpio/test/instance_client/test_aws_instance_client_options.h"

namespace google::scp::cpio {
/*! @copydoc InstanceClientInterface
 */
class TestAwsInstanceClient : public InstanceClient {
 public:
  explicit TestAwsInstanceClient(
      const std::shared_ptr<TestAwsInstanceClientOptions>& options)
      : InstanceClient(options) {}
};
}  // namespace google::scp::cpio
