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

#include "core/interface/async_context.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/queue_service/v1/queue_service.pb.h"

namespace google::scp::cpio::client_providers {

/**
 * @brief Interface responsible for queuing messages.
 */
class QueueClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~QueueClientProviderInterface() = default;
  /**
   * @brief Enqueue a message to the queue.
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult EnqueueMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context) noexcept = 0;
  /**
   * @brief Get top message from the queue.
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetTopMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context) noexcept = 0;
  /**
   * @brief Update expiration time of a message from the queue.
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult UpdateMessageExpirationTime(
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageExpirationTimeRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageExpirationTimeResponse>&
          update_message_expiration_time_context) noexcept = 0;
  /**
   * @brief Delete a message from the queue.
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult DeleteMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_message_context) noexcept = 0;
};

/// Configurations for QueueClient.
struct QueueClientOptions {
  virtual ~QueueClientOptions() = default;

  /**
   * @brief Required. The identifier of the queue. The queue is per client per
   * service. In AWS SQS, it's the queue name. In GCP Pub/Sub, there is only one
   * Subscription subscribes to the Topic, so the queue name is tied to Topic Id
   * and Subscription Id.
   *
   */
  std::string queue_name;
};

class QueueClientProviderFactory {
 public:
  /**
   * @brief Factory to create QueueClientProvider.
   *
   * @param options QueueClientOptions.
   * @return std::shared_ptr<QueueClientProviderInterface> created
   * QueueClientProviderProvider.
   */
  static std::shared_ptr<QueueClientProviderInterface> Create(
      const std::shared_ptr<QueueClientOptions>& options);
};
}  // namespace google::scp::cpio::client_providers
