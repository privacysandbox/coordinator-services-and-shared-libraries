// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpio/client_providers/queue_client_provider/src/aws/aws_queue_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include <aws/core/Aws.h>
#include <aws/sqs/SQSClient.h>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_provider.h"
#include "cpio/client_providers/queue_client_provider/mock/aws/mock_sqs_client.h"
#include "cpio/client_providers/queue_client_provider/src/aws/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

using Aws::InitAPI;
using Aws::NoResult;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Vector;
using Aws::Client::AWSError;
using Aws::Client::ClientConfiguration;
using Aws::SQS::SQSClient;
using Aws::SQS::SQSErrors;
using Aws::SQS::Model::ChangeMessageVisibilityOutcome;
using Aws::SQS::Model::ChangeMessageVisibilityRequest;
using Aws::SQS::Model::DeleteMessageOutcome;
using Aws::SQS::Model::GetQueueUrlOutcome;
using Aws::SQS::Model::GetQueueUrlResult;
using Aws::SQS::Model::Message;
using Aws::SQS::Model::QueueAttributeName;
using Aws::SQS::Model::ReceiveMessageOutcome;
using Aws::SQS::Model::ReceiveMessageRequest;
using Aws::SQS::Model::ReceiveMessageResult;
using Aws::SQS::Model::SendMessageOutcome;
using Aws::SQS::Model::SendMessageRequest;
using Aws::SQS::Model::SendMessageResult;
using google::cmrt::sdk::queue_service::v1::DeleteMessageRequest;
using google::cmrt::sdk::queue_service::v1::DeleteMessageResponse;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageRequest;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageResponse;
using google::cmrt::sdk::queue_service::v1::GetTopMessageRequest;
using google::cmrt::sdk::queue_service::v1::GetTopMessageResponse;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutRequest;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGE_NOT_IN_FLIGHT;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::AwsSqsClientFactory;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockSqsClient;
using std::atomic_bool;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::unique_ptr;
using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::Return;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kQueueName[] = "queue name";
constexpr char kQueueUrl[] = "queue url";
constexpr char kMessageBody[] = "message body";
constexpr char kMessageId[] = "message id";
constexpr char kReceiptInfo[] = "receipt info";
constexpr char kInvalidReceiptInfo[] = "";
const uint8_t kDefaultMaxNumberOfMessagesReceived = 1;
const uint8_t kDefaultMaxWaitTimeSeconds = 0;
const uint16_t kVisibilityTimeoutSeconds = 10;
const uint16_t kInvalidVisibilityTimeoutSeconds = 50000;
}  // namespace

namespace google::scp::cpio::client_providers::test {

class MockAwsSqsClientFactory : public AwsSqsClientFactory {
 public:
  MOCK_METHOD(shared_ptr<SQSClient>, CreateSqsClient,
              (const shared_ptr<ClientConfiguration> client_config),
              (noexcept, override));
};

class AwsQueueClientProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  AwsQueueClientProviderTest() {
    queue_client_options_ = make_shared<QueueClientOptions>();
    queue_client_options_->queue_name = kQueueName;

    mock_instance_client_ = make_shared<MockInstanceClientProvider>();
    mock_instance_client_->instance_resource_name = kResourceNameMock;
    mock_sqs_client_ = make_shared<NiceMock<MockSqsClient>>();

    GetQueueUrlResult get_queue_url_result;
    get_queue_url_result.SetQueueUrl(kQueueUrl);
    GetQueueUrlOutcome get_queue_url_outcome(move(get_queue_url_result));
    ON_CALL(*mock_sqs_client_, GetQueueUrl)
        .WillByDefault(Return(get_queue_url_outcome));

    mock_sqs_client_factory_ = make_shared<NiceMock<MockAwsSqsClientFactory>>();
    ON_CALL(*mock_sqs_client_factory_, CreateSqsClient)
        .WillByDefault(Return(mock_sqs_client_));

    enqueue_message_context_.request = make_shared<EnqueueMessageRequest>();
    enqueue_message_context_.callback = [this](auto) { finish_called_ = true; };

    get_top_message_context_.request = make_shared<GetTopMessageRequest>();
    get_top_message_context_.callback = [this](auto) { finish_called_ = true; };

    update_message_visibility_timeout_context_.request =
        make_shared<UpdateMessageVisibilityTimeoutRequest>();
    update_message_visibility_timeout_context_.callback = [this](auto) {
      finish_called_ = true;
    };

    delete_message_context_.request = make_shared<DeleteMessageRequest>();
    delete_message_context_.callback = [this](auto) { finish_called_ = true; };

    queue_client_provider_ = make_unique<AwsQueueClientProvider>(
        queue_client_options_, mock_instance_client_,
        make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
        mock_sqs_client_factory_);
  }

  void TearDown() override { EXPECT_SUCCESS(queue_client_provider_->Stop()); }

  shared_ptr<QueueClientOptions> queue_client_options_;
  shared_ptr<MockInstanceClientProvider> mock_instance_client_;
  shared_ptr<MockSqsClient> mock_sqs_client_;
  shared_ptr<MockAwsSqsClientFactory> mock_sqs_client_factory_;
  unique_ptr<AwsQueueClientProvider> queue_client_provider_;

  AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>
      enqueue_message_context_;
  AsyncContext<GetTopMessageRequest, GetTopMessageResponse>
      get_top_message_context_;
  AsyncContext<UpdateMessageVisibilityTimeoutRequest,
               UpdateMessageVisibilityTimeoutResponse>
      update_message_visibility_timeout_context_;
  AsyncContext<DeleteMessageRequest, DeleteMessageResponse>
      delete_message_context_;

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  atomic_bool finish_called_{false};
};

TEST_F(AwsQueueClientProviderTest, RunWithNullQueueClientOptions) {
  auto client = make_unique<AwsQueueClientProvider>(
      nullptr, mock_instance_client_, make_shared<MockAsyncExecutor>(),
      make_shared<MockAsyncExecutor>(), mock_sqs_client_factory_);

  EXPECT_SUCCESS(client->Init());
  EXPECT_THAT(client->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED)));
}

TEST_F(AwsQueueClientProviderTest, RunWithEmptyQueueName) {
  queue_client_options_->queue_name = "";
  auto client = make_unique<AwsQueueClientProvider>(
      queue_client_options_, mock_instance_client_,
      make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
      mock_sqs_client_factory_);

  EXPECT_SUCCESS(client->Init());
  EXPECT_THAT(client->Run(),
              ResultIs(FailureExecutionResult(
                  SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED)));
}

TEST_F(AwsQueueClientProviderTest, RunWithCreateClientConfigurationFailed) {
  auto failure_result = FailureExecutionResult(123);
  mock_instance_client_->get_instance_resource_name_mock = failure_result;
  auto client = make_unique<AwsQueueClientProvider>(
      queue_client_options_, mock_instance_client_,
      make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
      mock_sqs_client_factory_);

  EXPECT_SUCCESS(client->Init());
  EXPECT_THAT(client->Run(), ResultIs(failure_result));
}

TEST_F(AwsQueueClientProviderTest, RunWithGetQueueUrlFailed) {
  AWSError<SQSErrors> get_queue_url_error(SQSErrors::SERVICE_UNAVAILABLE,
                                          false);
  GetQueueUrlOutcome get_queue_url_outcome(get_queue_url_error);
  EXPECT_CALL(*mock_sqs_client_, GetQueueUrl)
      .WillOnce(Return(get_queue_url_outcome));

  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_THAT(queue_client_provider_->Run(),
              ResultIs(FailureExecutionResult(SC_AWS_SERVICE_UNAVAILABLE)));
}

MATCHER_P(HasGetQueueUrlRequestParams, queue_name, "") {
  return ExplainMatchResult(Eq(queue_name), arg.GetQueueName(),
                            result_listener);
}

TEST_F(AwsQueueClientProviderTest, RunSuccessWithExistingQueue) {
  GetQueueUrlResult get_queue_url_result;
  get_queue_url_result.SetQueueUrl(kQueueUrl);
  GetQueueUrlOutcome get_queue_url_outcome(move(get_queue_url_result));
  EXPECT_CALL(*mock_sqs_client_,
              GetQueueUrl(HasGetQueueUrlRequestParams(kQueueName)))
      .WillOnce(Return(get_queue_url_outcome));

  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());
}

MATCHER_P2(HasSendMessageRequestParams, queue_url, message_body, "") {
  return ExplainMatchResult(Eq(queue_url), arg.GetQueueUrl(),
                            result_listener) &&
         ExplainMatchResult(Eq(message_body), arg.GetMessageBody(),
                            result_listener);
}

TEST_F(AwsQueueClientProviderTest, EnqueueMessageSuccess) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  enqueue_message_context_.request->set_message_body(kMessageBody);
  enqueue_message_context_.callback =
      [this](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                 enqueue_message_context) {
        EXPECT_SUCCESS(enqueue_message_context.result);

        EXPECT_EQ(enqueue_message_context.response->message_id(), kMessageId);
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_sqs_client_,
              SendMessageAsync(
                  HasSendMessageRequestParams(kQueueUrl, kMessageBody), _, _))
      .WillOnce([](auto, auto callback, auto) {
        SendMessageRequest send_message_request;
        SendMessageResult send_message_result;
        send_message_result.SetMessageId(kMessageId);
        SendMessageOutcome send_message_outcome(move(send_message_result));
        callback(nullptr, send_message_request, move(send_message_outcome),
                 nullptr);
      });

  EXPECT_SUCCESS(
      queue_client_provider_->EnqueueMessage(enqueue_message_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsQueueClientProviderTest, EnqueueMessageCallbackFailed) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  enqueue_message_context_.request->set_message_body(kMessageBody);
  enqueue_message_context_.callback =
      [this](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                 enqueue_message_context) {
        EXPECT_THAT(
            enqueue_message_context.result,
            ResultIs(FailureExecutionResult(SC_AWS_INVALID_CREDENTIALS)));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_sqs_client_,
              SendMessageAsync(
                  HasSendMessageRequestParams(kQueueUrl, kMessageBody), _, _))
      .WillOnce([](auto, auto callback, auto) {
        SendMessageRequest send_message_request;
        AWSError<SQSErrors> sqs_error(SQSErrors::INVALID_CLIENT_TOKEN_ID,
                                      false);
        SendMessageOutcome send_message_outcome(sqs_error);
        callback(nullptr, send_message_request, move(send_message_outcome),
                 nullptr);
      });

  EXPECT_SUCCESS(
      queue_client_provider_->EnqueueMessage(enqueue_message_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P3(HasReceiveMessageRequestParams, queue_url, max_number_of_messages,
           wait_time_seconds, "") {
  return ExplainMatchResult(Eq(queue_url), arg.GetQueueUrl(),
                            result_listener) &&
         ExplainMatchResult(Eq(max_number_of_messages),
                            arg.GetMaxNumberOfMessages(), result_listener) &&
         ExplainMatchResult(Eq(wait_time_seconds), arg.GetWaitTimeSeconds(),
                            result_listener);
}

TEST_F(AwsQueueClientProviderTest, GetTopMessageSuccess) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_SUCCESS(get_top_message_context.result);

        EXPECT_EQ(get_top_message_context.response->message_id(), kMessageId);
        EXPECT_EQ(get_top_message_context.response->message_body(),
                  kMessageBody);
        EXPECT_EQ(get_top_message_context.response->receipt_info(),
                  kReceiptInfo);
        finish_called_ = true;
      };

  EXPECT_CALL(
      *mock_sqs_client_,
      ReceiveMessageAsync(HasReceiveMessageRequestParams(
                              kQueueUrl, kDefaultMaxNumberOfMessagesReceived,
                              kDefaultMaxWaitTimeSeconds),
                          _, _))
      .WillOnce([](auto, auto callback, auto) {
        ReceiveMessageRequest receive_message_request;
        Message message;
        message.SetMessageId(kMessageId);
        message.SetBody(kMessageBody);
        message.SetReceiptHandle(kReceiptInfo);
        Vector<Message> messages;
        messages.push_back(message);
        ReceiveMessageResult receive_message_result;
        receive_message_result.SetMessages(messages);
        ReceiveMessageOutcome receive_message_outcome(
            move(receive_message_result));
        callback(nullptr, receive_message_request,
                 move(receive_message_outcome), nullptr);
      });

  EXPECT_SUCCESS(
      queue_client_provider_->GetTopMessage(get_top_message_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsQueueClientProviderTest, GetTopMessageCallbackFailed) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_THAT(get_top_message_context.result,
                    ResultIs(FailureExecutionResult(SC_AWS_VALIDATION_FAILED)));
        finish_called_ = true;
      };

  EXPECT_CALL(
      *mock_sqs_client_,
      ReceiveMessageAsync(HasReceiveMessageRequestParams(
                              kQueueUrl, kDefaultMaxNumberOfMessagesReceived,
                              kDefaultMaxWaitTimeSeconds),
                          _, _))
      .WillOnce([](auto, auto callback, auto) {
        ReceiveMessageRequest receive_message_request;
        AWSError<SQSErrors> sqs_error(SQSErrors::VALIDATION, false);
        ReceiveMessageOutcome receive_message_outcome(sqs_error);
        callback(nullptr, receive_message_request,
                 move(receive_message_outcome), nullptr);
      });

  EXPECT_SUCCESS(
      queue_client_provider_->GetTopMessage(get_top_message_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsQueueClientProviderTest, GotTopMessageCallbackWithNoMessage) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_SUCCESS(get_top_message_context.result);
        // Returns empty response.
        EXPECT_TRUE(get_top_message_context.response->message_id().empty());
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_sqs_client_, ReceiveMessageAsync(_, _, _))
      .WillOnce([](auto, auto callback, auto) {
        ReceiveMessageRequest receive_message_request;
        Vector<Message> messages;
        ReceiveMessageResult receive_message_result;
        receive_message_result.SetMessages(messages);
        ReceiveMessageOutcome receive_message_outcome(
            move(receive_message_result));
        callback(nullptr, receive_message_request,
                 move(receive_message_outcome), nullptr);
      });

  EXPECT_SUCCESS(
      queue_client_provider_->GetTopMessage(get_top_message_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsQueueClientProviderTest, GetTopMessageCallbackWithMulitpleMessages) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_THAT(
            get_top_message_context.result,
            ResultIs(FailureExecutionResult(
                SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED)));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_sqs_client_, ReceiveMessageAsync(_, _, _))
      .WillOnce([](auto, auto callback, auto) {
        ReceiveMessageRequest receive_message_request;
        Message message1;
        message1.SetMessageId(kMessageId);
        message1.SetBody(kMessageBody);
        message1.SetReceiptHandle(kReceiptInfo);
        Message message2;
        message2.SetMessageId("123");
        message2.SetBody("456");
        message2.SetReceiptHandle("789");
        Vector<Message> messages;
        messages.push_back(message1);
        messages.push_back(message2);
        ReceiveMessageResult receive_message_result;
        receive_message_result.SetMessages(messages);
        ReceiveMessageOutcome receive_message_outcome(
            move(receive_message_result));
        callback(nullptr, receive_message_request,
                 move(receive_message_outcome), nullptr);
      });

  EXPECT_SUCCESS(
      queue_client_provider_->GetTopMessage(get_top_message_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P3(HasChangeVisibilityRequestParams, queue_url, visibility_timeout,
           receipt_handle, "") {
  return ExplainMatchResult(Eq(queue_url), arg.GetQueueUrl(),
                            result_listener) &&
         ExplainMatchResult(Eq(visibility_timeout), arg.GetVisibilityTimeout(),
                            result_listener) &&
         ExplainMatchResult(Eq(receipt_handle), arg.GetReceiptHandle(),
                            result_listener);
}

TEST_F(AwsQueueClientProviderTest, UpdateMessageVisibilityTimeoutSuccess) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kVisibilityTimeoutSeconds);
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_SUCCESS(update_message_visibility_timeout_context.result);
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_sqs_client_,
              ChangeMessageVisibilityAsync(
                  HasChangeVisibilityRequestParams(
                      kQueueUrl, kVisibilityTimeoutSeconds, kReceiptInfo),
                  _, _))
      .WillOnce([](auto, auto callback, auto) {
        ChangeMessageVisibilityRequest change_message_visibility_request;
        NoResult result;
        ChangeMessageVisibilityOutcome change_message_visibility_outcome(
            result);
        callback(nullptr, change_message_visibility_request,
                 move(change_message_visibility_outcome), nullptr);
      });

  EXPECT_SUCCESS(queue_client_provider_->UpdateMessageVisibilityTimeout(
      update_message_visibility_timeout_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutWithInvalidReceiptInfo) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kInvalidReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kInvalidVisibilityTimeoutSeconds);

  EXPECT_THAT(queue_client_provider_->UpdateMessageVisibilityTimeout(
                  update_message_visibility_timeout_context_),
              ResultIs(FailureExecutionResult(
                  SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO)));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutWithInvalidExpirationTime) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kInvalidVisibilityTimeoutSeconds);

  EXPECT_THAT(queue_client_provider_->UpdateMessageVisibilityTimeout(
                  update_message_visibility_timeout_context_),
              ResultIs(FailureExecutionResult(
                  SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT)));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutCallbackFailed) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kVisibilityTimeoutSeconds);
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_THAT(update_message_visibility_timeout_context.result,
                    ResultIs(FailureExecutionResult(SC_AWS_INVALID_REQUEST)));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_sqs_client_,
              ChangeMessageVisibilityAsync(
                  HasChangeVisibilityRequestParams(
                      kQueueUrl, kVisibilityTimeoutSeconds, kReceiptInfo),
                  _, _))
      .WillOnce([](auto, auto callback, auto) {
        ChangeMessageVisibilityRequest change_message_visibility_request;
        AWSError<SQSErrors> sqs_error(SQSErrors::MALFORMED_QUERY_STRING, false);
        ChangeMessageVisibilityOutcome change_message_visibility_outcome(
            sqs_error);
        callback(nullptr, change_message_visibility_request,
                 move(change_message_visibility_outcome), nullptr);
      });

  EXPECT_SUCCESS(queue_client_provider_->UpdateMessageVisibilityTimeout(
      update_message_visibility_timeout_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P2(HasDeleteMessageRequestParams, queue_url, receipt_handle, "") {
  return ExplainMatchResult(Eq(queue_url), arg.GetQueueUrl(),
                            result_listener) &&
         ExplainMatchResult(Eq(receipt_handle), arg.GetReceiptHandle(),
                            result_listener);
}

TEST_F(AwsQueueClientProviderTest, DeleteMessageSuccess) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  delete_message_context_.request->set_receipt_info(kReceiptInfo);
  delete_message_context_.callback =
      [this](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                 delete_message_context) {
        EXPECT_SUCCESS(delete_message_context.result);
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_sqs_client_,
              DeleteMessageAsync(
                  HasDeleteMessageRequestParams(kQueueUrl, kReceiptInfo), _, _))
      .WillOnce([](auto, auto callback, auto) {
        Aws::SQS::Model::DeleteMessageRequest delete_message_request;
        NoResult result;
        DeleteMessageOutcome delete_message_outcome(result);
        callback(nullptr, delete_message_request, move(delete_message_outcome),
                 nullptr);
      });

  EXPECT_SUCCESS(
      queue_client_provider_->DeleteMessage(delete_message_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(AwsQueueClientProviderTest, DeleteMessageCallbackFailed) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  delete_message_context_.request->set_receipt_info(kReceiptInfo);
  delete_message_context_.callback =
      [this](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                 delete_message_context) {
        EXPECT_THAT(delete_message_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGE_NOT_IN_FLIGHT)));
        finish_called_ = true;
      };

  EXPECT_CALL(*mock_sqs_client_,
              DeleteMessageAsync(
                  HasDeleteMessageRequestParams(kQueueUrl, kReceiptInfo), _, _))
      .WillOnce([](auto, auto callback, auto) {
        Aws::SQS::Model::DeleteMessageRequest delete_message_request;
        AWSError<SQSErrors> sqs_error(SQSErrors::MESSAGE_NOT_INFLIGHT, false);
        DeleteMessageOutcome delete_message_outcome(sqs_error);
        callback(nullptr, delete_message_request, move(delete_message_outcome),
                 nullptr);
      });

  EXPECT_SUCCESS(
      queue_client_provider_->DeleteMessage(delete_message_context_));
  WaitUntil([this]() { return finish_called_.load(); });
}

}  // namespace google::scp::cpio::client_providers::test
