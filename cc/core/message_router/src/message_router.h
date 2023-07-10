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

#include <map>
#include <memory>
#include <string>

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/interface/async_context.h"
#include "core/interface/message_router_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core {
template <>
struct MessageTraits<google::protobuf::Any> {
  typedef std::string TypeIdentifier;
};

/*! @copydoc MessageRouterInterface
 */
class MessageRouter : public MessageRouterInterface<google::protobuf::Any,
                                                    google::protobuf::Any> {
 public:
  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  void OnMessageReceived(
      const std::shared_ptr<
          AsyncContext<google::protobuf::Any, google::protobuf::Any>>&
          context) noexcept override;

  using AsyncAction = typename std::function<void(
      AsyncContext<google::protobuf::Any, google::protobuf::Any>&)>;
  ExecutionResult Subscribe(const RequestTypeId& request_type,
                            const AsyncAction& action) noexcept override;

 private:
  // TODO(b/229794047): Figures out a better way to store the request_type to
  // have a better performance.
  common::ConcurrentMap<std::string, AsyncAction> actions_;
};
}  // namespace google::scp::core
