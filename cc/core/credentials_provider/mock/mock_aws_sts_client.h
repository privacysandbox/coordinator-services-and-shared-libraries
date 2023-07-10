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
#include <string>

#include <aws/sts/STSClient.h>

namespace google::scp::core::credentials_provider::mock {

class MockSTSClient : public Aws::STS::STSClient {
 public:
  MockSTSClient() : STSClient() {}

  void AssumeRoleAsync(
      const Aws::STS::Model::AssumeRoleRequest& request,
      const Aws::STS::AssumeRoleResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (mock_assume_role_async) {
      mock_assume_role_async(request, handler, context);
      return;
    }

    STSClient::AssumeRoleAsync(request, handler, context);
  }

  std::function<void(
      const Aws::STS::Model::AssumeRoleRequest&,
      const Aws::STS::AssumeRoleResponseReceivedHandler&,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)>
      mock_assume_role_async;
};
}  // namespace google::scp::core::credentials_provider::mock
