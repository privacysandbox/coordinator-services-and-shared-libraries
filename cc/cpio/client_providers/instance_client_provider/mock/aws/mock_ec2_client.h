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

#include <gmock/gmock.h>

#include <memory>

#include <aws/ec2/EC2Client.h>
#include <aws/ec2/model/DescribeInstancesRequest.h>
#include <aws/ec2/model/DescribeTagsRequest.h>

namespace google::scp::cpio::client_providers::mock {
class MockEC2Client : public Aws::EC2::EC2Client {
 public:
  MOCK_METHOD(void, DescribeInstancesAsync,
              (const Aws::EC2::Model::DescribeInstancesRequest&,
               const Aws::EC2::DescribeInstancesResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));

  MOCK_METHOD(void, DescribeTagsAsync,
              (const Aws::EC2::Model::DescribeTagsRequest&,
               const Aws::EC2::DescribeTagsResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));
};

}  // namespace google::scp::cpio::client_providers::mock
