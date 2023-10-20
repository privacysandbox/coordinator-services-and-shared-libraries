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

#include "gcp_queue_client_provider.h"

#include <string>

#include <grpcpp/grpcpp.h>

#include "absl/strings/str_format.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_utils.h"
#include "cpio/common/src/gcp/gcp_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

#include "error_codes.h"

using absl::StrFormat;
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
using google::protobuf::Empty;
using google::pubsub::v1::AcknowledgeRequest;
using google::pubsub::v1::ModifyAckDeadlineRequest;
using google::pubsub::v1::Publisher;
using google::pubsub::v1::PublishRequest;
using google::pubsub::v1::PublishResponse;
using google::pubsub::v1::PubsubMessage;
using google::pubsub::v1::PullRequest;
using google::pubsub::v1::PullResponse;
using google::pubsub::v1::Subscriber;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_CONFIG_VISIBILITY_TIMEOUT;
using google::scp::core::errors::SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_MISMATCH;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::common::GcpUtils;
using grpc::Channel;
using grpc::ChannelArguments;
using grpc::ClientContext;
using grpc::CreateChannel;
using grpc::CreateCustomChannel;
using grpc::GoogleDefaultCredentials;
using grpc::StatusCode;
using grpc::StubOptions;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::placeholders::_1;

static constexpr char kGcpQueueClientProvider[] = "GcpQueueClientProvider";
static constexpr char kPubSubEndpointUri[] = "pubsub.googleapis.com";
static constexpr char kGcpTopicFormatString[] = "projects/%s/topics/%s";
static constexpr char kGcpSubscriptionFormatString[] =
    "projects/%s/subscriptions/%s";
static constexpr uint8_t kMaxNumberOfMessagesReceived = 1;
static constexpr uint16_t kMaxAckDeadlineSeconds = 600;

namespace google::scp::cpio::client_providers {

ExecutionResult GcpQueueClientProvider::Init() noexcept {
  ExecutionResult execution_result(SuccessExecutionResult());
  if (!queue_client_options_) {
    execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED);
    SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, execution_result,
              "Invalid queue client options.");
    return execution_result;
  }

  if (queue_client_options_->queue_name.empty()) {
    execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED);
    SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, execution_result,
              "Invalid queue name.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult GcpQueueClientProvider::Run() noexcept {
  auto project_id_or =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client_provider_);
  if (!project_id_or.Successful()) {
    SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, project_id_or.result(),
              "Failed to get project ID for current instance");
    return project_id_or.result();
  }
  project_id_ = move(*project_id_or);

  publisher_stub_ =
      pubsub_stub_factory_->CreatePublisherStub(queue_client_options_);
  if (!publisher_stub_) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED);
    SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, execution_result,
              "Failed to create publisher.");
    return execution_result;
  }

  subscriber_stub_ =
      pubsub_stub_factory_->CreateSubscriberStub(queue_client_options_);
  if (!subscriber_stub_) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED);
    SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, execution_result,
              "Failed to create subscriber.");
    return execution_result;
  }

  topic_name_ = StrFormat(kGcpTopicFormatString, project_id_,
                          queue_client_options_->queue_name);
  subscription_name_ = StrFormat(kGcpSubscriptionFormatString, project_id_,
                                 queue_client_options_->queue_name);

  return SuccessExecutionResult();
}

ExecutionResult GcpQueueClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpQueueClientProvider::EnqueueMessage(
    AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
        enqueue_message_context) noexcept {
  if (enqueue_message_context.request->message_body().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, enqueue_message_context, execution_result,
        "Failed to enqueue message due to missing message body in "
        "the request for topic: %s",
        topic_name_.c_str());
    enqueue_message_context.result = execution_result;
    enqueue_message_context.Finish();
    return execution_result;
  }

  auto execution_result = io_async_executor_->Schedule(
      bind(&GcpQueueClientProvider::EnqueueMessageAsync, this,
           enqueue_message_context),
      AsyncPriority::Normal);
  if (!execution_result.Successful()) {
    enqueue_message_context.result = execution_result;
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, enqueue_message_context,
        enqueue_message_context.result,
        "Enqueue Message request failed to be scheduled. Topic: %s",
        topic_name_.c_str());
    enqueue_message_context.Finish();
    return execution_result;
  }
  return SuccessExecutionResult();
}

void GcpQueueClientProvider::EnqueueMessageAsync(
    AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
        enqueue_message_context) noexcept {
  PublishRequest publish_request;
  publish_request.set_topic(topic_name_);
  PubsubMessage* message = publish_request.add_messages();
  message->set_data(enqueue_message_context.request->message_body().c_str());

  ClientContext client_context;
  PublishResponse publish_response;
  auto status = publisher_stub_->Publish(&client_context, publish_request,
                                         &publish_response);
  if (!status.ok()) {
    auto execution_result = GcpUtils::GcpErrorConverter(status);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, enqueue_message_context, execution_result,
        "Failed to enqueue message due to GCP Pub/Sub service error. Topic: %s",
        topic_name_.c_str());
    FinishContext(execution_result, enqueue_message_context,
                  cpu_async_executor_);
    return;
  }

  const auto& message_ids = publish_response.message_ids();
  // This should never happen.
  if (message_ids.size() != 1) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_MISMATCH);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, enqueue_message_context, execution_result,
        "The number of message ids recevied from the response does "
        "not match the number of message in the request. Topic: %s",
        topic_name_.c_str());
    FinishContext(execution_result, enqueue_message_context,
                  cpu_async_executor_);
    return;
  }

  auto response = make_shared<EnqueueMessageResponse>();
  response->set_message_id(publish_response.message_ids(0));
  enqueue_message_context.response = move(response);
  FinishContext(SuccessExecutionResult(), enqueue_message_context,
                cpu_async_executor_);
}

ExecutionResult GcpQueueClientProvider::GetTopMessage(
    AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
        get_top_message_context) noexcept {
  auto execution_result = io_async_executor_->Schedule(
      bind(&GcpQueueClientProvider::GetTopMessageAsync, this,
           get_top_message_context),
      AsyncPriority::Normal);
  if (!execution_result.Successful()) {
    get_top_message_context.result = execution_result;
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, get_top_message_context,
        get_top_message_context.result,
        "Get Top Message request failed to be scheduled. Topic: %s",
        topic_name_.c_str());
    get_top_message_context.Finish();
    return execution_result;
  }
  return SuccessExecutionResult();
}

void GcpQueueClientProvider::GetTopMessageAsync(
    AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
        get_top_message_context) noexcept {
  PullRequest pull_request;
  pull_request.set_subscription(subscription_name_);
  pull_request.set_max_messages(kMaxNumberOfMessagesReceived);
  ClientContext client_context;
  PullResponse pull_response;
  auto status =
      subscriber_stub_->Pull(&client_context, pull_request, &pull_response);

  if (!status.ok()) {
    auto execution_result = GcpUtils::GcpErrorConverter(status);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, get_top_message_context, execution_result,
        "Failed to get top message due to GCP Pub/Sub service error. "
        "Subscription: %s",
        subscription_name_.c_str());
    FinishContext(execution_result, get_top_message_context,
                  cpu_async_executor_);
    return;
  }

  const auto& received_messages = pull_response.received_messages();

  // This should never happen.
  if (received_messages.size() > kMaxNumberOfMessagesReceived) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, get_top_message_context, execution_result,
        "The number of messages recevied from the response is larger "
        "than the maximum number. Subscription: %s",
        subscription_name_.c_str());
    FinishContext(execution_result, get_top_message_context,
                  cpu_async_executor_);
    return;
  }

  if (received_messages.empty()) {
    get_top_message_context.response = make_shared<GetTopMessageResponse>();
    FinishContext(SuccessExecutionResult(), get_top_message_context,
                  cpu_async_executor_);
    return;
  }

  auto received_message = received_messages.at(0);
  auto response = make_shared<GetTopMessageResponse>();
  response->set_message_body(received_message.mutable_message()->data());
  response->set_message_id(received_message.mutable_message()->message_id());
  response->set_receipt_info(received_message.ack_id());
  get_top_message_context.response = move(response);

  FinishContext(SuccessExecutionResult(), get_top_message_context,
                cpu_async_executor_);
}

ExecutionResult GcpQueueClientProvider::UpdateMessageVisibilityTimeout(
    AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                 UpdateMessageVisibilityTimeoutResponse>&
        update_message_visibility_timeout_context) noexcept {
  if (update_message_visibility_timeout_context.request->receipt_info()
          .empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to update message visibility timeout due to missing "
        "receipt info in the request. Subscription: %s",
        subscription_name_.c_str());
    update_message_visibility_timeout_context.result = execution_result;
    update_message_visibility_timeout_context.Finish();
    return execution_result;
  }

  auto lifetime_in_seconds = update_message_visibility_timeout_context.request
                                 ->message_visibility_timeout()
                                 .seconds();
  if (lifetime_in_seconds < 0 || lifetime_in_seconds > kMaxAckDeadlineSeconds) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to update message visibility timeout due to invalid "
        "message visibility timeout in the request. Subscription: %s",
        subscription_name_.c_str());
    update_message_visibility_timeout_context.result = execution_result;
    update_message_visibility_timeout_context.Finish();
    return execution_result;
  }

  auto execution_result = io_async_executor_->Schedule(
      bind(&GcpQueueClientProvider::UpdateMessageVisibilityTimeoutAsync, this,
           update_message_visibility_timeout_context),
      AsyncPriority::Normal);
  if (!execution_result.Successful()) {
    update_message_visibility_timeout_context.result = execution_result;
    SCP_ERROR_CONTEXT(kGcpQueueClientProvider,
                      update_message_visibility_timeout_context,
                      update_message_visibility_timeout_context.result,
                      "Update message visibility timeout request failed to be "
                      "scheduled for subscription: %s",
                      subscription_name_.c_str());
    update_message_visibility_timeout_context.Finish();
    return execution_result;
  }
  return SuccessExecutionResult();
}

void GcpQueueClientProvider::UpdateMessageVisibilityTimeoutAsync(
    AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                 UpdateMessageVisibilityTimeoutResponse>&
        update_message_visibility_timeout_context) noexcept {
  ModifyAckDeadlineRequest modify_ack_deadline_request;
  modify_ack_deadline_request.set_subscription(subscription_name_);
  modify_ack_deadline_request.add_ack_ids(
      update_message_visibility_timeout_context.request->receipt_info()
          .c_str());
  modify_ack_deadline_request.set_ack_deadline_seconds(
      update_message_visibility_timeout_context.request
          ->message_visibility_timeout()
          .seconds());

  ClientContext client_context;
  Empty modify_ack_deadline_response;
  auto status = subscriber_stub_->ModifyAckDeadline(
      &client_context, modify_ack_deadline_request,
      &modify_ack_deadline_response);
  if (!status.ok()) {
    auto execution_result = GcpUtils::GcpErrorConverter(status);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to modify message ack deadline due to GCP Pub/Sub "
        "service error. Subscription: %s",
        subscription_name_.c_str());
    FinishContext(execution_result, update_message_visibility_timeout_context,
                  cpu_async_executor_);
    return;
  }

  update_message_visibility_timeout_context.response =
      make_shared<UpdateMessageVisibilityTimeoutResponse>();
  FinishContext(SuccessExecutionResult(),
                update_message_visibility_timeout_context, cpu_async_executor_);
}

ExecutionResult GcpQueueClientProvider::DeleteMessage(
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_message_context) noexcept {
  if (delete_message_context.request->receipt_info().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE);
    SCP_ERROR_CONTEXT(kGcpQueueClientProvider, delete_message_context,
                      execution_result,
                      "Failed to delete message due to missing receipt info in "
                      "the request. Subscription: %s",
                      subscription_name_.c_str());
    delete_message_context.result = execution_result;
    delete_message_context.Finish();
    return execution_result;
  }

  auto execution_result = io_async_executor_->Schedule(
      bind(&GcpQueueClientProvider::DeleteMessageAsync, this,
           delete_message_context),
      AsyncPriority::Normal);
  if (!execution_result.Successful()) {
    delete_message_context.result = execution_result;
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, delete_message_context,
        delete_message_context.result,
        "Delete request failed to be scheduled for subscription: %s",
        subscription_name_.c_str());
    delete_message_context.Finish();
    return execution_result;
  }
  return SuccessExecutionResult();
}

void GcpQueueClientProvider::DeleteMessageAsync(
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_message_context) noexcept {
  AcknowledgeRequest acknowledge_request;
  acknowledge_request.set_subscription(subscription_name_);
  acknowledge_request.add_ack_ids(
      delete_message_context.request->receipt_info().c_str());

  ClientContext client_context;
  Empty acknowledge_response;
  auto status = subscriber_stub_->Acknowledge(
      &client_context, acknowledge_request, &acknowledge_response);
  if (!status.ok()) {
    auto execution_result = GcpUtils::GcpErrorConverter(status);
    SCP_ERROR_CONTEXT(kGcpQueueClientProvider, delete_message_context,
                      execution_result,
                      "Failed to acknowledge message due to GCP Pub/Sub "
                      "service error. Subscription: %s",
                      subscription_name_.c_str());
    FinishContext(execution_result, delete_message_context,
                  cpu_async_executor_);
    return;
  }

  delete_message_context.response = make_shared<DeleteMessageResponse>();
  FinishContext(SuccessExecutionResult(), delete_message_context,
                cpu_async_executor_);
}

shared_ptr<Channel> GcpPubSubStubFactory::GetPubSubChannel(
    const std::shared_ptr<QueueClientOptions>& options) noexcept {
  if (!channel_) {
    ChannelArguments args;
    args.SetInt(GRPC_ARG_ENABLE_RETRIES, 1);  // enable
    channel_ = CreateCustomChannel(kPubSubEndpointUri,
                                   GoogleDefaultCredentials(), args);
  }
  return channel_;
}

shared_ptr<Publisher::StubInterface> GcpPubSubStubFactory::CreatePublisherStub(
    const shared_ptr<QueueClientOptions>& options) noexcept {
  return unique_ptr<Publisher::Stub>(
      Publisher::NewStub(GetPubSubChannel(options), StubOptions()));
}

shared_ptr<Subscriber::StubInterface>
GcpPubSubStubFactory::CreateSubscriberStub(
    const shared_ptr<QueueClientOptions>& options) noexcept {
  return unique_ptr<Subscriber::Stub>(
      Subscriber::NewStub(GetPubSubChannel(options), StubOptions()));
}

#ifndef TEST_CPIO
shared_ptr<QueueClientProviderInterface> QueueClientProviderFactory::Create(
    const shared_ptr<QueueClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface> instance_client,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  return make_shared<GcpQueueClientProvider>(
      options, instance_client, cpu_async_executor, io_async_executor);
}
#endif
}  // namespace google::scp::cpio::client_providers
