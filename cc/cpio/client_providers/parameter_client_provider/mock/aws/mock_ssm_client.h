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
#include <vector>

#include <aws/ssm/SSMClient.h>
#include <aws/ssm/model/GetParametersRequest.h>

namespace google::scp::cpio::client_providers::mock {
class MockSSMClient : public Aws::SSM::SSMClient {
 public:
  void GetParametersAsync(
      const Aws::SSM::Model::GetParametersRequest& request,
      const Aws::SSM::GetParametersResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (IsEqual(request.GetNames(), get_parameters_request_mock.GetNames())) {
      handler(this, request, get_parameters_outcome_mock, context);
      return;
    }
    Aws::SSM::Model::GetParametersResult result;
    handler(this, request, Aws::SSM::Model::GetParametersOutcome(result),
            context);
  }

  Aws::SSM::Model::GetParametersRequest get_parameters_request_mock;
  Aws::SSM::Model::GetParametersOutcome get_parameters_outcome_mock;

 private:
  bool IsEqual(const Aws::Vector<Aws::String>& v1,
               const Aws::Vector<Aws::String>& v2) const {
    return std::is_permutation(v1.begin(), v1.end(), v2.begin());
  }
};
}  // namespace google::scp::cpio::client_providers::mock
