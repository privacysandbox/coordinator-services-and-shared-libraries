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

#include "async_context.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {
template <typename T>
struct MessageTraits {
  typedef void TypeIdentifier;
};

/**
 * @brief MessageRouter is responsible for dispatching the coming requests to
 * the correct action. The client needs to provide the action by subscribing to
 * it.
 */
template <typename TRequest, typename TResponse>
class MessageRouterInterface : public ServiceInterface {
 public:
  virtual ~MessageRouterInterface() = default;

  /// Type of action function the client provides.
  using AsyncAction =
      typename std::function<void(AsyncContext<TRequest, TResponse>&)>;
  using RequestType = TRequest;
  using ResponseType = TResponse;
  using RequestTypeId = typename MessageTraits<TRequest>::TypeIdentifier;

  /**
   * @brief Notified that message is received.
   *
   * @param context context of the message.
   * @return ExecutionResult result of message processing.
   */
  virtual void OnMessageReceived(
      const std::shared_ptr<AsyncContext<TRequest, TResponse>>&
          context) noexcept = 0;

  // TODO(b/229794047): Figures out a better way to store the request_type to
  // have a better performance.
  /**
   * @brief Subscribes the action function for the specific request type.
   *
   * @param request_type the request type the client is subsribed for.
   * @param action the action function for the specific request.
   * @return ExecutionResult the result of the subscription.
   */
  virtual ExecutionResult Subscribe(const RequestTypeId& request_type,
                                    const AsyncAction& action) noexcept = 0;
};
}  // namespace google::scp::core
