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

#include <aws/sqs/SQSClient.h>
#include <aws/sqs/model/ChangeMessageVisibilityRequest.h>
#include <aws/sqs/model/CreateQueueRequest.h>
#include <aws/sqs/model/DeleteMessageRequest.h>
#include <aws/sqs/model/GetQueueUrlRequest.h>
#include <aws/sqs/model/ReceiveMessageRequest.h>
#include <aws/sqs/model/SendMessageRequest.h>

namespace google::scp::cpio::client_providers::mock {

// Even though this is a mock, the default constructor also calls the default
// constructor of SQSClient which MUST be called after InitSDK.
class MockSqsClient : public Aws::SQS::SQSClient {
 public:
  MOCK_METHOD(Aws::SQS::Model::GetQueueUrlOutcome, GetQueueUrl,
              (const Aws::SQS::Model::GetQueueUrlRequest&), (const, override));
  MOCK_METHOD(void, SendMessageAsync,
              (const Aws::SQS::Model::SendMessageRequest&,
               const Aws::SQS::SendMessageResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));
  MOCK_METHOD(void, ReceiveMessageAsync,
              (const Aws::SQS::Model::ReceiveMessageRequest&,
               const Aws::SQS::ReceiveMessageResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));
  MOCK_METHOD(void, ChangeMessageVisibilityAsync,
              (const Aws::SQS::Model::ChangeMessageVisibilityRequest&,
               const Aws::SQS::ChangeMessageVisibilityResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));
  MOCK_METHOD(void, DeleteMessageAsync,
              (const Aws::SQS::Model::DeleteMessageRequest&,
               const Aws::SQS::DeleteMessageResponseReceivedHandler&,
               const std::shared_ptr<const Aws::Client::AsyncCallerContext>&),
              (const, override));
};

}  // namespace google::scp::cpio::client_providers::mock
