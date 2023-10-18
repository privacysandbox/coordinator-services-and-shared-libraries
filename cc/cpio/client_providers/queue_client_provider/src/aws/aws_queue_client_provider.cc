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

#include "aws_queue_client_provider.h"

#include <string>

#include <aws/sqs/model/ChangeMessageVisibilityRequest.h>
#include <aws/sqs/model/DeleteMessageRequest.h>
#include <aws/sqs/model/GetQueueUrlRequest.h>
#include <aws/sqs/model/ReceiveMessageRequest.h>
#include <aws/sqs/model/SendMessageRequest.h>

#include "aws/sqs/SQSClient.h"
#include "core/async_executor/src/aws/aws_async_executor.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_utils.h"
#include "cpio/common/src/aws/aws_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

#include "error_codes.h"
#include "sqs_error_converter.h"

using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::SQS::SQSClient;
using Aws::SQS::SQSErrors;
using Aws::SQS::Model::ChangeMessageVisibilityOutcome;
using Aws::SQS::Model::ChangeMessageVisibilityRequest;
using Aws::SQS::Model::DeleteMessageOutcome;
using Aws::SQS::Model::GetQueueUrlRequest;
using Aws::SQS::Model::QueueAttributeName;
using Aws::SQS::Model::ReceiveMessageOutcome;
using Aws::SQS::Model::ReceiveMessageRequest;
using Aws::SQS::Model::SendMessageOutcome;
using Aws::SQS::Model::SendMessageRequest;
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
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using google::scp::cpio::common::CreateClientConfiguration;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

static constexpr char kAwsQueueClientProvider[] = "AwsQueueClientProvider";
static const uint8_t kMaxNumberOfMessagesReceived = 1;
static const uint8_t kMaxWaitTimeSeconds = 0;
static const uint16_t kMaxVisibilityTimeoutSeconds = 600;

namespace google::scp::cpio::client_providers {
ExecutionResult AwsQueueClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsQueueClientProvider::Run() noexcept {
  ExecutionResult execution_result(SuccessExecutionResult());
  if (!queue_client_options_) {
    execution_result = FailureExecutionResult(
        SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED);
    SCP_ERROR(kAwsQueueClientProvider, kZeroUuid, execution_result,
              "Invalid queue client options.");
    return execution_result;
  }

  if (queue_client_options_->queue_name.empty()) {
    execution_result = FailureExecutionResult(
        SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED);
    SCP_ERROR(kAwsQueueClientProvider, kZeroUuid, execution_result,
              "Invalid queue name.");
    return execution_result;
  }

  auto client_config_or = CreateClientConfiguration();
  if (!client_config_or.Successful()) {
    execution_result = client_config_or.result();
    SCP_ERROR(kAwsQueueClientProvider, kZeroUuid, execution_result,
              "Failed to create ClientConfiguration");
    return execution_result;
  }

  sqs_client_ = sqs_client_factory_->CreateSqsClient(move(*client_config_or));

  auto queue_url_or = GetQueueUrl();
  if (!queue_url_or.Successful() || queue_url_or->empty()) {
    execution_result = queue_url_or.result();
    SCP_ERROR(kAwsQueueClientProvider, kZeroUuid, execution_result,
              "Failed to get queue url.");
    return execution_result;
  }
  queue_url_ = move(*queue_url_or);

  return execution_result;
}

ExecutionResultOr<shared_ptr<ClientConfiguration>>
AwsQueueClientProvider::CreateClientConfiguration() noexcept {
  auto region_code_or =
      AwsInstanceClientUtils::GetCurrentRegionCode(instance_client_provider_);
  if (!region_code_or.Successful()) {
    SCP_ERROR(kAwsQueueClientProvider, kZeroUuid, region_code_or.result(),
              "Failed to get region code for current instance");
    return region_code_or.result();
  }

  auto client_config = common::CreateClientConfiguration(
      make_shared<string>(move(*region_code_or)));
  client_config->executor = make_shared<AwsAsyncExecutor>(io_async_executor_);

  return client_config;
}

ExecutionResultOr<string> AwsQueueClientProvider::GetQueueUrl() noexcept {
  GetQueueUrlRequest get_queue_url_request;
  get_queue_url_request.SetQueueName(queue_client_options_->queue_name.c_str());

  auto get_queue_url_outcome = sqs_client_->GetQueueUrl(get_queue_url_request);
  if (!get_queue_url_outcome.IsSuccess()) {
    auto error_type = get_queue_url_outcome.GetError().GetErrorType();
    auto error_message = get_queue_url_outcome.GetError().GetMessage().c_str();
    auto execution_result = SqsErrorConverter::ConvertSqsError(error_type);
    SCP_ERROR(
        kAwsQueueClientProvider, kZeroUuid, execution_result,
        "Failed to get queue url due to AWS SQS service error. Error code: "
        "%d, error message: %s",
        error_type, error_message);
    return execution_result;
  }
  return get_queue_url_outcome.GetResult().GetQueueUrl().c_str();
}

ExecutionResult AwsQueueClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsQueueClientProvider::EnqueueMessage(
    AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
        enqueue_message_context) noexcept {
  const string& message_body = enqueue_message_context.request->message_body();
  if (message_body.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE);
    SCP_ERROR_CONTEXT(kAwsQueueClientProvider, enqueue_message_context,
                      execution_result,
                      "Failed to send message due to missing message body");
    enqueue_message_context.result = execution_result;
    enqueue_message_context.Finish();
    return execution_result;
  }

  SendMessageRequest send_message_request;
  send_message_request.SetQueueUrl(queue_url_.c_str());
  send_message_request.SetMessageBody(message_body.c_str());

  sqs_client_->SendMessageAsync(
      send_message_request,
      bind(&AwsQueueClientProvider::OnSendMessageCallback, this,
           enqueue_message_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsQueueClientProvider::OnSendMessageCallback(
    AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
        enqueue_message_context,
    const SQSClient* sqs_client, const SendMessageRequest& send_message_request,
    SendMessageOutcome send_message_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  auto execution_result = SuccessExecutionResult();
  if (!send_message_outcome.IsSuccess()) {
    auto error_type = send_message_outcome.GetError().GetErrorType();
    auto error_message = send_message_outcome.GetError().GetMessage().c_str();
    execution_result = SqsErrorConverter::ConvertSqsError(error_type);
    SCP_ERROR_CONTEXT(
        kAwsQueueClientProvider, enqueue_message_context, execution_result,
        "Failed to send message due to AWS SQS service error. Error "
        "code: %d, error message: %s",
        error_type, error_message);
    FinishContext(execution_result, enqueue_message_context,
                  cpu_async_executor_);
    return;
  }
  enqueue_message_context.response = make_shared<EnqueueMessageResponse>();
  enqueue_message_context.response->set_message_id(
      send_message_outcome.GetResult().GetMessageId().c_str());
  FinishContext(execution_result, enqueue_message_context, cpu_async_executor_);
}

ExecutionResult AwsQueueClientProvider::GetTopMessage(
    AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
        get_top_message_context) noexcept {
  ReceiveMessageRequest receive_message_request;
  receive_message_request.SetQueueUrl(queue_url_.c_str());
  receive_message_request.SetMaxNumberOfMessages(kMaxNumberOfMessagesReceived);
  receive_message_request.SetWaitTimeSeconds(kMaxWaitTimeSeconds);
  sqs_client_->ReceiveMessageAsync(
      receive_message_request,
      bind(&AwsQueueClientProvider::OnReceiveMessageCallback, this,
           get_top_message_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsQueueClientProvider::OnReceiveMessageCallback(
    AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
        get_top_message_context,
    const SQSClient* sqs_client,
    const ReceiveMessageRequest& receive_message_request,
    ReceiveMessageOutcome receive_message_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  auto execution_result = SuccessExecutionResult();
  if (!receive_message_outcome.IsSuccess()) {
    auto error_type = receive_message_outcome.GetError().GetErrorType();
    auto error_message =
        receive_message_outcome.GetError().GetMessage().c_str();
    execution_result = SqsErrorConverter::ConvertSqsError(error_type);
    SCP_ERROR_CONTEXT(
        kAwsQueueClientProvider, get_top_message_context, execution_result,
        "Failed to receive message due to AWS SQS service error. Error "
        "code: %d, error message: %s",
        error_type, error_message);
    FinishContext(execution_result, get_top_message_context,
                  cpu_async_executor_);
    return;
  }

  const auto& messages = receive_message_outcome.GetResult().GetMessages();
  auto response = make_shared<GetTopMessageResponse>();
  if (messages.size() == 0) {
    SCP_INFO_CONTEXT(kAwsQueueClientProvider, get_top_message_context,
                     "No messages received from the queue.");
    get_top_message_context.response = move(response);
    FinishContext(execution_result, get_top_message_context,
                  cpu_async_executor_);
    return;
  }

  // This should never happen.
  if (messages.size() > kMaxNumberOfMessagesReceived) {
    execution_result = FailureExecutionResult(
        SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED);
    SCP_ERROR_CONTEXT(
        kAwsQueueClientProvider, get_top_message_context, execution_result,
        "The number of messages recevies from the queue is higher "
        "than the maximum number. Messages count: %d",
        messages.size());
    FinishContext(execution_result, get_top_message_context,
                  cpu_async_executor_);
    return;
  }

  const auto& message = messages[0];
  response->set_message_id(message.GetMessageId().c_str());
  response->set_message_body(message.GetBody().c_str());
  response->set_receipt_info(message.GetReceiptHandle().c_str());
  get_top_message_context.response = move(response);
  FinishContext(execution_result, get_top_message_context, cpu_async_executor_);
}

ExecutionResult AwsQueueClientProvider::UpdateMessageVisibilityTimeout(
    AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                 UpdateMessageVisibilityTimeoutResponse>&
        update_message_visibility_timeout_context) noexcept {
  const string& receipt_info =
      update_message_visibility_timeout_context.request->receipt_info();
  if (receipt_info.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO);
    SCP_ERROR_CONTEXT(
        kAwsQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to update visibility timeout of the message due to "
        "missing receipt info");
    update_message_visibility_timeout_context.result = execution_result;
    update_message_visibility_timeout_context.Finish();
    return execution_result;
  }

  const int64_t lifetime = update_message_visibility_timeout_context.request
                               ->message_visibility_timeout()
                               .seconds();
  if (lifetime < 0 || lifetime > kMaxVisibilityTimeoutSeconds) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT);
    SCP_ERROR_CONTEXT(
        kAwsQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to update visibility tiemout of the message due to "
        "invalid lifetime time");
    update_message_visibility_timeout_context.result = execution_result;
    update_message_visibility_timeout_context.Finish();
    return execution_result;
  }

  ChangeMessageVisibilityRequest change_message_visibility_request;
  change_message_visibility_request.SetQueueUrl(queue_url_.c_str());
  change_message_visibility_request.SetVisibilityTimeout(lifetime);
  change_message_visibility_request.SetReceiptHandle(receipt_info.c_str());

  sqs_client_->ChangeMessageVisibilityAsync(
      change_message_visibility_request,
      bind(&AwsQueueClientProvider::OnChangeMessageVisibilityCallback, this,
           update_message_visibility_timeout_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsQueueClientProvider::OnChangeMessageVisibilityCallback(
    AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                 UpdateMessageVisibilityTimeoutResponse>&
        update_message_visibility_timeout_context,
    const SQSClient* sqs_client,
    const ChangeMessageVisibilityRequest& change_message_visibility_request,
    ChangeMessageVisibilityOutcome change_message_visibility_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  auto execution_result = SuccessExecutionResult();
  if (!change_message_visibility_outcome.IsSuccess()) {
    auto error_type =
        change_message_visibility_outcome.GetError().GetErrorType();
    auto error_message =
        change_message_visibility_outcome.GetError().GetMessage().c_str();
    execution_result = SqsErrorConverter::ConvertSqsError(error_type);
    SCP_ERROR_CONTEXT(
        kAwsQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to change message visibility due to AWS SQS service "
        "error. Error code: %d, error message: %s",
        error_type, error_message);
  }

  FinishContext(execution_result, update_message_visibility_timeout_context,
                cpu_async_executor_);
}

ExecutionResult AwsQueueClientProvider::DeleteMessage(
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_message_context) noexcept {
  const string& receipt_info = delete_message_context.request->receipt_info();
  if (receipt_info.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO);
    SCP_ERROR_CONTEXT(kAwsQueueClientProvider, delete_message_context,
                      execution_result,
                      "Failed to delete message due to missing receipt info");
    delete_message_context.result = execution_result;
    delete_message_context.Finish();
    return execution_result;
  }

  Aws::SQS::Model::DeleteMessageRequest delete_message_request;
  delete_message_request.SetQueueUrl(queue_url_.c_str());
  delete_message_request.SetReceiptHandle(receipt_info.c_str());

  sqs_client_->DeleteMessageAsync(
      delete_message_request,
      bind(&AwsQueueClientProvider::OnDeleteMessageCallback, this,
           delete_message_context, _1, _2, _3, _4),
      nullptr);

  return SuccessExecutionResult();
}

void AwsQueueClientProvider::OnDeleteMessageCallback(
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_message_context,
    const SQSClient* sqs_client,
    const Aws::SQS::Model::DeleteMessageRequest& delete_message_request,
    DeleteMessageOutcome delete_message_outcome,
    const shared_ptr<const AsyncCallerContext> async_context) noexcept {
  auto execution_result = SuccessExecutionResult();
  if (!delete_message_outcome.IsSuccess()) {
    auto error_type = delete_message_outcome.GetError().GetErrorType();
    auto error_message = delete_message_outcome.GetError().GetMessage().c_str();
    execution_result = SqsErrorConverter::ConvertSqsError(error_type);
    SCP_ERROR_CONTEXT(kAwsQueueClientProvider, delete_message_context,
                      execution_result,
                      "Failed to delete message due to AWS SQS service error. "
                      "Error code: %d, error message: %s",
                      error_type, error_message);
  }

  FinishContext(execution_result, delete_message_context, cpu_async_executor_);
}

shared_ptr<SQSClient> AwsSqsClientFactory::CreateSqsClient(
    const shared_ptr<ClientConfiguration> client_config) noexcept {
  return make_shared<SQSClient>(*client_config);
}

shared_ptr<QueueClientProviderInterface> QueueClientProviderFactory::Create(
    const shared_ptr<QueueClientOptions>& options,
    const shared_ptr<InstanceClientProviderInterface> instance_client,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  return make_shared<AwsQueueClientProvider>(
      options, instance_client, cpu_async_executor, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
