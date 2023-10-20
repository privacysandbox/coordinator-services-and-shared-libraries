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

#include "cc/cpio/client_providers/queue_client_provider/src/gcp/gcp_queue_client_provider.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <google/pubsub/v1/pubsub.grpc.pb.h>

#include "cc/cpio/client_providers/queue_client_provider/src/gcp/error_codes.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "cpio/client_providers/queue_client_provider/mock/gcp/mock_pubsub_stubs.h"
#include "cpio/client_providers/queue_client_provider/src/gcp/error_codes.h"
#include "cpio/common/src/gcp/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

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
using google::pubsub::v1::Publisher;
using google::pubsub::v1::Subscriber;
using google::scp::core::AsyncContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_ABORTED;
using google::scp::core::errors::SC_GCP_DATA_LOSS;
using google::scp::core::errors::SC_GCP_FAILED_PRECONDITION;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::errors::SC_GCP_PERMISSION_DENIED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_CONFIG_VISIBILITY_TIMEOUT;
using google::scp::core::errors::SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockPublisherStub;
using google::scp::cpio::client_providers::mock::MockSubscriberStub;
using grpc::Status;
using grpc::StatusCode;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::UnorderedElementsAre;

static constexpr char kInstanceResourceName[] =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
static constexpr char kQueueName[] = "queue_name";
static constexpr char kMessageBody[] = "message_body";
static constexpr char kMessageId[] = "message_id";
static constexpr char kReceiptInfo[] = "receipt_info";
static constexpr char kExpectedTopicName[] =
    "projects/123456789/topics/queue_name";
static constexpr char kExpectedSubscriptionName[] =
    "projects/123456789/subscriptions/queue_name";
static constexpr uint8_t kMaxNumberOfMessagesReceived = 1;
static constexpr uint16_t kAckDeadlineSeconds = 60;
static constexpr uint16_t kInvalidAckDeadlineSeconds = 1200;

namespace google::scp::cpio::client_providers::gcp_queue_client::test {

class MockGcpPubSubStubFactory : public GcpPubSubStubFactory {
 public:
  MOCK_METHOD(shared_ptr<Publisher::StubInterface>, CreatePublisherStub,
              (const std::shared_ptr<QueueClientOptions>&),
              (noexcept, override));
  MOCK_METHOD(shared_ptr<Subscriber::StubInterface>, CreateSubscriberStub,
              (const std::shared_ptr<QueueClientOptions>&),
              (noexcept, override));
};

class GcpQueueClientProviderTest : public ::testing::Test {
 protected:
  GcpQueueClientProviderTest() {
    queue_client_options_ = make_shared<QueueClientOptions>();
    queue_client_options_->queue_name = kQueueName;
    mock_instance_client_provider_ = make_shared<MockInstanceClientProvider>();
    mock_instance_client_provider_->instance_resource_name =
        kInstanceResourceName;

    mock_publisher_stub_ = make_shared<NiceMock<MockPublisherStub>>();
    mock_subscriber_stub_ = make_shared<NiceMock<MockSubscriberStub>>();

    mock_pubsub_stub_factory_ =
        make_shared<NiceMock<MockGcpPubSubStubFactory>>();
    ON_CALL(*mock_pubsub_stub_factory_, CreatePublisherStub)
        .WillByDefault(Return(mock_publisher_stub_));
    ON_CALL(*mock_pubsub_stub_factory_, CreateSubscriberStub)
        .WillByDefault(Return(mock_subscriber_stub_));

    queue_client_provider_ = make_unique<GcpQueueClientProvider>(
        queue_client_options_, mock_instance_client_provider_,
        make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
        mock_pubsub_stub_factory_);

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
  }

  void TearDown() override { EXPECT_SUCCESS(queue_client_provider_->Stop()); }

  shared_ptr<QueueClientOptions> queue_client_options_;
  shared_ptr<MockInstanceClientProvider> mock_instance_client_provider_;
  shared_ptr<MockPublisherStub> mock_publisher_stub_;
  shared_ptr<MockSubscriberStub> mock_subscriber_stub_;
  shared_ptr<MockGcpPubSubStubFactory> mock_pubsub_stub_factory_;
  unique_ptr<GcpQueueClientProvider> queue_client_provider_;

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
  std::atomic_bool finish_called_{false};
};

TEST_F(GcpQueueClientProviderTest, InitWithNullQueueClientOptions) {
  auto client = make_unique<GcpQueueClientProvider>(
      nullptr, mock_instance_client_provider_, make_shared<MockAsyncExecutor>(),
      make_shared<MockAsyncExecutor>(), mock_pubsub_stub_factory_);

  EXPECT_THAT(client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED)));
}

TEST_F(GcpQueueClientProviderTest, InitWithEmptyQueueName) {
  queue_client_options_->queue_name = "";
  auto client = make_unique<GcpQueueClientProvider>(
      queue_client_options_, mock_instance_client_provider_,
      make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
      mock_pubsub_stub_factory_);

  EXPECT_THAT(client->Init(),
              ResultIs(FailureExecutionResult(
                  SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED)));
}

TEST_F(GcpQueueClientProviderTest, InitWithGetProjectIdFailure) {
  mock_instance_client_provider_->get_instance_resource_name_mock =
      FailureExecutionResult(123);
  auto client = make_unique<GcpQueueClientProvider>(
      queue_client_options_, mock_instance_client_provider_,
      make_shared<MockAsyncExecutor>(), make_shared<MockAsyncExecutor>(),
      mock_pubsub_stub_factory_);

  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_THAT(client->Run(), ResultIs(FailureExecutionResult(123)));
}

TEST_F(GcpQueueClientProviderTest, InitWithPublisherCreationFailure) {
  EXPECT_CALL(*mock_pubsub_stub_factory_, CreatePublisherStub)
      .WillOnce(Return(nullptr));

  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_THAT(queue_client_provider_->Run(),
              ResultIs(FailureExecutionResult(
                  SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED)));
}

TEST_F(GcpQueueClientProviderTest, InitWithSubscriberCreationFailure) {
  EXPECT_CALL(*mock_pubsub_stub_factory_, CreateSubscriberStub)
      .WillOnce(Return(nullptr));

  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_THAT(queue_client_provider_->Run(),
              ResultIs(FailureExecutionResult(
                  SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED)));
}

MATCHER_P(MessageHasBody, message_body, "") {
  return ExplainMatchResult(Eq(message_body), arg.data(), result_listener);
}

MATCHER_P2(HasPublishParams, topic_name, message_body, "") {
  return ExplainMatchResult(Eq(topic_name), arg.topic(), result_listener) &&
         ExplainMatchResult(UnorderedElementsAre(MessageHasBody(message_body)),
                            arg.messages(), result_listener);
}

TEST_F(GcpQueueClientProviderTest, EnqueueMessageSuccess) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(*mock_publisher_stub_,
              Publish(_, HasPublishParams(kExpectedTopicName, kMessageBody), _))
      .WillOnce([](auto, auto, auto* publish_response) {
        publish_response->add_message_ids(kMessageId);
        return Status(StatusCode::OK, "");
      });
  enqueue_message_context_.request->set_message_body(kMessageBody);
  enqueue_message_context_.callback =
      [this](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                 enqueue_message_context) {
        EXPECT_SUCCESS(enqueue_message_context.result);

        EXPECT_EQ(enqueue_message_context.response->message_id(), kMessageId);
        finish_called_ = true;
      };

  EXPECT_SUCCESS(
      queue_client_provider_->EnqueueMessage(enqueue_message_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest, EnqueueMessageFailureWithEmptyMessageBody) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  enqueue_message_context_.request->set_message_body("");
  enqueue_message_context_.callback =
      [this](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                 enqueue_message_context) {
        EXPECT_THAT(enqueue_message_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

        finish_called_ = true;
      };

  EXPECT_THAT(queue_client_provider_->EnqueueMessage(enqueue_message_context_),
              ResultIs(FailureExecutionResult(
                  SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest, EnqueueMessageFailureWithPubSubError) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(*mock_publisher_stub_,
              Publish(_, HasPublishParams(kExpectedTopicName, kMessageBody), _))
      .WillOnce(Return(Status(StatusCode::PERMISSION_DENIED, "")));

  enqueue_message_context_.request->set_message_body(kMessageBody);
  enqueue_message_context_.callback =
      [this](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                 enqueue_message_context) {
        EXPECT_THAT(enqueue_message_context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_PERMISSION_DENIED)));

        finish_called_ = true;
      };

  EXPECT_SUCCESS(
      queue_client_provider_->EnqueueMessage(enqueue_message_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P2(HasPullParams, subscription_name, max_messages, "") {
  return ExplainMatchResult(Eq(subscription_name), arg.subscription(),
                            result_listener) &&
         ExplainMatchResult(Eq(max_messages), arg.max_messages(),
                            result_listener);
}

TEST_F(GcpQueueClientProviderTest, GetTopMessageSuccess) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(*mock_subscriber_stub_,
              Pull(_,
                   HasPullParams(kExpectedSubscriptionName,
                                 kMaxNumberOfMessagesReceived),
                   _))
      .WillOnce([](auto, auto, auto* pull_response) {
        auto* received_message = pull_response->add_received_messages();
        received_message->mutable_message()->set_data(kMessageBody);
        received_message->mutable_message()->set_message_id(kMessageId);
        received_message->set_ack_id(kReceiptInfo);
        return Status(StatusCode::OK, "");
      });

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

  EXPECT_SUCCESS(
      queue_client_provider_->GetTopMessage(get_top_message_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest, GetTopMessageWithNoMessagesReturns) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(*mock_subscriber_stub_,
              Pull(_,
                   HasPullParams(kExpectedSubscriptionName,
                                 kMaxNumberOfMessagesReceived),
                   _))
      .WillOnce([](auto, auto, auto* pull_response) {
        return Status(StatusCode::OK, "");
      });

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_SUCCESS(get_top_message_context.result);

        finish_called_ = true;
      };

  EXPECT_SUCCESS(
      queue_client_provider_->GetTopMessage(get_top_message_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest, GetTopMessageFailureWithPubSubError) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(*mock_subscriber_stub_,
              Pull(_,
                   HasPullParams(kExpectedSubscriptionName,
                                 kMaxNumberOfMessagesReceived),
                   _))
      .WillOnce(Return(Status(StatusCode::ABORTED, "")));

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_THAT(get_top_message_context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_ABORTED)));

        finish_called_ = true;
      };

  EXPECT_SUCCESS(
      queue_client_provider_->GetTopMessage(get_top_message_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest,
       GetTopMessageFailureWithNumberOfMessagesReceivedExceedingLimit) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(*mock_subscriber_stub_,
              Pull(_,
                   HasPullParams(kExpectedSubscriptionName,
                                 kMaxNumberOfMessagesReceived),
                   _))
      .WillOnce([](auto, auto, auto* pull_response) {
        pull_response->add_received_messages();
        pull_response->add_received_messages();
        return Status(StatusCode::OK, "");
      });

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_THAT(
            get_top_message_context.result,
            ResultIs(FailureExecutionResult(
                SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED)));

        finish_called_ = true;
      };

  EXPECT_SUCCESS(
      queue_client_provider_->GetTopMessage(get_top_message_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P3(HasModifyAckDeadlineParams, subscription_name, ack_id,
           ack_deadline_seconds, "") {
  return ExplainMatchResult(Eq(subscription_name), arg.subscription(),
                            result_listener) &&
         ExplainMatchResult(UnorderedElementsAre(Eq(ack_id)), arg.ack_ids(),
                            result_listener) &&
         ExplainMatchResult(Eq(ack_deadline_seconds),
                            arg.ack_deadline_seconds(), result_listener);
}

TEST_F(GcpQueueClientProviderTest, UpdateMessageVisibilityTimeoutSuccess) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(*mock_subscriber_stub_,
              ModifyAckDeadline(
                  _,
                  HasModifyAckDeadlineParams(kExpectedSubscriptionName,
                                             kReceiptInfo, kAckDeadlineSeconds),
                  _))
      .WillOnce(Return(Status(StatusCode::OK, "")));

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kAckDeadlineSeconds);
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_SUCCESS(update_message_visibility_timeout_context.result);

        finish_called_ = true;
      };

  EXPECT_SUCCESS(queue_client_provider_->UpdateMessageVisibilityTimeout(
      update_message_visibility_timeout_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutFailureWithEmptyReceiptInfo) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  update_message_visibility_timeout_context_.request->set_receipt_info("");
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_THAT(update_message_visibility_timeout_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

        finish_called_ = true;
      };

  EXPECT_THAT(queue_client_provider_->UpdateMessageVisibilityTimeout(
                  update_message_visibility_timeout_context_),
              ResultIs(FailureExecutionResult(
                  SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutFailureWithInvalidMessageLifetime) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kInvalidAckDeadlineSeconds);
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_THAT(
            update_message_visibility_timeout_context.result,
            ResultIs(FailureExecutionResult(
                SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT)));

        finish_called_ = true;
      };

  EXPECT_THAT(queue_client_provider_->UpdateMessageVisibilityTimeout(
                  update_message_visibility_timeout_context_),
              ResultIs(FailureExecutionResult(
                  SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT)));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutFailureWithPubSubError) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(*mock_subscriber_stub_,
              ModifyAckDeadline(
                  _,
                  HasModifyAckDeadlineParams(kExpectedSubscriptionName,
                                             kReceiptInfo, kAckDeadlineSeconds),
                  _))
      .WillOnce(Return(Status(StatusCode::FAILED_PRECONDITION, "")));

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kAckDeadlineSeconds);
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_THAT(
            update_message_visibility_timeout_context.result,
            ResultIs(FailureExecutionResult(SC_GCP_FAILED_PRECONDITION)));

        finish_called_ = true;
      };

  EXPECT_SUCCESS(queue_client_provider_->UpdateMessageVisibilityTimeout(
      update_message_visibility_timeout_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

MATCHER_P2(HasAcknowledgeParams, subscription_name, ack_id, "") {
  return ExplainMatchResult(Eq(subscription_name), arg.subscription(),
                            result_listener) &&
         ExplainMatchResult(UnorderedElementsAre(Eq(ack_id)), arg.ack_ids(),
                            result_listener);
}

TEST_F(GcpQueueClientProviderTest, DeleteMessageSuccess) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(
      *mock_subscriber_stub_,
      Acknowledge(
          _, HasAcknowledgeParams(kExpectedSubscriptionName, kReceiptInfo), _))
      .WillOnce(Return(Status(StatusCode::OK, "")));

  delete_message_context_.request->set_receipt_info(kReceiptInfo);
  delete_message_context_.callback =
      [this](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                 delete_message_context) {
        EXPECT_SUCCESS(delete_message_context.result);

        finish_called_ = true;
      };

  EXPECT_SUCCESS(
      queue_client_provider_->DeleteMessage(delete_message_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest, DeleteMessageFailureWithEmptyReceiptInfo) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  delete_message_context_.request->set_receipt_info("");
  delete_message_context_.callback =
      [this](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                 delete_message_context) {
        EXPECT_THAT(delete_message_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

        finish_called_ = true;
      };

  EXPECT_THAT(queue_client_provider_->DeleteMessage(delete_message_context_),
              ResultIs(FailureExecutionResult(
                  SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

  WaitUntil([this]() { return finish_called_.load(); });
}

TEST_F(GcpQueueClientProviderTest, DeleteMessageFailureWithPubSubError) {
  EXPECT_SUCCESS(queue_client_provider_->Init());
  EXPECT_SUCCESS(queue_client_provider_->Run());

  EXPECT_CALL(
      *mock_subscriber_stub_,
      Acknowledge(
          _, HasAcknowledgeParams(kExpectedSubscriptionName, kReceiptInfo), _))
      .WillOnce(Return(Status(StatusCode::DATA_LOSS, "")));

  delete_message_context_.request->set_receipt_info(kReceiptInfo);
  delete_message_context_.callback =
      [this](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                 delete_message_context) {
        EXPECT_THAT(delete_message_context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_DATA_LOSS)));

        finish_called_ = true;
      };

  EXPECT_SUCCESS(
      queue_client_provider_->DeleteMessage(delete_message_context_));

  WaitUntil([this]() { return finish_called_.load(); });
}

}  // namespace google::scp::cpio::client_providers::gcp_queue_client::test
