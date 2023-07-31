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
#include <vector>

#include "async_context.h"
#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {
/**
 * @brief The main protocol to execute transactions for a specific component. If
 * a component supports 2-phase commit transaction, it must implement the
 * following class.
 *
 * @tparam TPrepareRequest represents the prepare request object class type.
 * @tparam TPrepareResponse represents the prepare response object class type.
 * @tparam TCommitRequest represents the commit request object class type.
 * @tparam TCommitResponse represents the commit response object class type.
 * @tparam TNotifyRequest represents the notify request object class type.
 * @tparam TNotifyResponse represents the notify response object class type.
 * @tparam TAbortRequest represents the abort request object class type.
 * @tparam TAbortResponse represents the abort response object class type.
 */
template <class TPrepareRequest, class TPrepareResponse, class TCommitRequest,
          class TCommitResponse, class TNotifyRequest, class TNotifyResponse,
          class TAbortRequest, class TAbortResponse>
class TransactionProtocolInterface {
 public:
  virtual ~TransactionProtocolInterface() = default;

  /**
   * @brief Prepares the component for a specific transaction. This is a
   * read operation and does not modify data.
   *
   * @param prepare_context the async context of Prepare operation.
   * operation.
   * @return core::ExecutionResult the result of the operation.
   */
  virtual core::ExecutionResult Prepare(
      core::AsyncContext<TPrepareRequest, TPrepareResponse>&
          prepare_context) noexcept = 0;

  /**
   * @brief Commits changes to the component for a specfic transaction. This
   * is a write operation and inserts a pending change.
   *
   * @param commit_context the async context of Commit operation.
   * @return core::ExecutionResult the result of the operation.
   */
  virtual core::ExecutionResult Commit(
      core::AsyncContext<TCommitRequest, TCommitResponse>&
          commit_context) noexcept = 0;

  /**
   * @brief Notifies the component to finish the pending operation. This is
   * a write operation and finalizes the prior state of the object.
   *
   * @param notify_context the async context of Notify operation.
   * @return core::ExecutionResult the result of the operation.
   */
  virtual core::ExecutionResult Notify(
      core::AsyncContext<TNotifyRequest, TNotifyResponse>&
          notify_context) noexcept = 0;

  /**
   * @brief Aborts all the pending operations to the component. This is a write
   * operation and cancels all the pending changes and rolls back the state.
   *
   * @param abort_context the async context of Abort operation.
   * @return core::ExecutionResult the result of the operation.
   */
  virtual core::ExecutionResult Abort(
      core::AsyncContext<TAbortRequest, TAbortResponse>&
          abort_context) noexcept = 0;
};
}  // namespace google::scp::core
