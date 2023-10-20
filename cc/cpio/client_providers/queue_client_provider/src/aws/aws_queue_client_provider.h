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
#include <string>
#include <utility>

#include "aws/sqs/SQSClient.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class AwsSqsClientFactory;

/*! @copydoc QueueClientProviderInterface
 */
class AwsQueueClientProvider : public QueueClientProviderInterface {
 public:
  virtual ~AwsQueueClientProvider() = default;

  explicit AwsQueueClientProvider(
      const std::shared_ptr<QueueClientOptions>& queue_client_options,
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor,
      const std::shared_ptr<AwsSqsClientFactory>& sqs_client_factory =
          std::make_shared<AwsSqsClientFactory>())
      : queue_client_options_(queue_client_options),
        instance_client_provider_(instance_client_provider),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        sqs_client_factory_(sqs_client_factory) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult EnqueueMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context) noexcept override;

  core::ExecutionResult GetTopMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context) noexcept override;

  core::ExecutionResult UpdateMessageVisibilityTimeout(
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>&
          update_message_visibility_timeout_context) noexcept override;

  core::ExecutionResult DeleteMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_message_context) noexcept override;

 private:
  /**
   * @brief Creates a Client Configuration object.
   *
   * @return core::ExecutionResultOr<std::shared_ptr<ClientConfiguration>> the
   * AWS client configuration.
   */
  core::ExecutionResultOr<std::shared_ptr<Aws::Client::ClientConfiguration>>
  CreateClientConfiguration() noexcept;

  /**
   * @brief Get Queue URL in AWS SQS.
   *
   * @return core::ExecutionResultOr<<std::string> the queue url.
   */
  core::ExecutionResultOr<std::string> GetQueueUrl() noexcept;

  /**
   * @brief Is called when the object is returned from the SQS SendMessage
   * callback.
   *
   * @param enqueue_message_context The enqueue message context object.
   * @param sqs_client An instance of the SQS client.
   * @param send_message_request The send message request.
   * @param send_message_outcome The send message outcome of the async
   * operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnSendMessageCallback(
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context,
      const Aws::SQS::SQSClient* sqs_client,
      const Aws::SQS::Model::SendMessageRequest& send_message_request,
      Aws::SQS::Model::SendMessageOutcome send_message_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when the object is returned from the SQS ReceiveMessage
   * callback.
   *
   * @param get_top_message_context The get top message context object.
   * @param sqs_client An instance of the SQS client.
   * @param receive_message_request The receive message request.
   * @param receive_message_outcome The receive message outcome of the async
   * operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnReceiveMessageCallback(
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context,
      const Aws::SQS::SQSClient* sqs_client,
      const Aws::SQS::Model::ReceiveMessageRequest& receive_message_request,
      Aws::SQS::Model::ReceiveMessageOutcome receive_message_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when the object is returned from the SQS Change Message
   * Visibility callback.
   *
   * @param update_message_visibility_timeout_context The update message
   * visibility timeout context object.
   * @param sqs_client An instance of the SQS client.
   * @param change_message_visibility_request The change message visibility
   * request.
   * @param change_message_visibility_outcome The change message visibility
   * outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnChangeMessageVisibilityCallback(
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>&
          update_message_visibility_timeout_context,
      const Aws::SQS::SQSClient* sqs_client,
      const Aws::SQS::Model::ChangeMessageVisibilityRequest&
          change_message_visibility_request,
      Aws::SQS::Model::ChangeMessageVisibilityOutcome
          change_message_visibility_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when the object is returned from the SQS Delete Message
   * callback.
   *
   * @param delete_message_context The delete message context object.
   * @param sqs_client An instance of the SQS client.
   * @param delete_message_request The delete message request.
   * @param delete_message_outcome The delete message outcome of the async
   * operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnDeleteMessageCallback(
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_message_context,
      const Aws::SQS::SQSClient* sqs_client,
      const Aws::SQS::Model::DeleteMessageRequest& delete_message_request,
      Aws::SQS::Model::DeleteMessageOutcome delete_message_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /// The configuration for queue client.
  std::shared_ptr<QueueClientOptions> queue_client_options_;

  /// The instance client provider.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;

  /// The instance of the async executor.
  const std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;

  /// Queue URL of the current queue.
  std::string queue_url_;

  /// An Instance of the AWS SQS client factory.
  std::shared_ptr<AwsSqsClientFactory> sqs_client_factory_;

  /// An Instance of the AWS SQS client.
  std::shared_ptr<Aws::SQS::SQSClient> sqs_client_;
};

/// Provides AwsSqsClient.
class AwsSqsClientFactory {
 public:
  /**
   * @brief Creates AWS SQS Client.
   *
   * @return std::shared_ptr<Aws::SQS::SQSClient> the creation result.
   */
  virtual std::shared_ptr<Aws::SQS::SQSClient> CreateSqsClient(
      const std::shared_ptr<Aws::Client::ClientConfiguration>
          client_config) noexcept;

  virtual ~AwsSqsClientFactory() = default;
};

}  // namespace google::scp::cpio::client_providers
