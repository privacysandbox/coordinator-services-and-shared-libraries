/*
 * Copyright 2023 Google LLC
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

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/common/lru_cache/src/lru_cache.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "roma/interface/roma.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/worker_api/src/worker_api.h"
#include "roma/sandbox/worker_pool/src/worker_pool.h"

#include "error_codes.h"
#include "request_converter.h"
#include "request_validator.h"

namespace google::scp::roma::sandbox::dispatcher {
class Dispatcher : public core::ServiceInterface {
 public:
  Dispatcher(std::shared_ptr<core::AsyncExecutor>& async_executor,
             std::shared_ptr<worker_pool::WorkerPool>& worker_pool,
             size_t max_pending_requests, size_t code_version_cache_size)
      : async_executor_(async_executor),
        worker_pool_(worker_pool),
        worker_index_(0),
        pending_requests_(0),
        max_pending_requests_(max_pending_requests),
        code_object_cache_(code_version_cache_size) {
    CHECK(max_pending_requests > 0) << "max_pending_requests cannot be zero";
    CHECK(code_version_cache_size > 0)
        << "code_version_cache_size cannot be zero";
  }

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  /**
   * @brief Enqueues a request to be handled by the workers.
   *
   * @tparam RequestT The type of the request.
   * @param request The request.
   * @param callback The function to call once the request completes.
   * @return core::ExecutionResult Whether the enqueue operation succeeded or
   * not.
   */
  template <typename RequestT>
  core::ExecutionResult Dispatch(std::unique_ptr<RequestT> request,
                                 Callback callback) noexcept {
    return InternalDispatch(std::move(request), callback);
  }

  /**
   * @brief Dispatch a set of requests. This funciton will block until all the
   * requests have been dispatched. This uses Dispatch.
   *
   * @tparam RequestT The type of the request.
   * @param batch The input bacth of request to enqueue.
   * @param batch_callback The callback to invoke once the batch is done.
   * @return core::ExecutionResult Whether the dispatch batch operation
   * succeeded or failed.
   */
  template <typename RequestT>
  core::ExecutionResult DispatchBatch(std::vector<RequestT>& batch,
                                      BatchCallback batch_callback) noexcept {
    auto batch_size = batch.size();
    auto batch_response =
        std::make_shared<std::vector<absl::StatusOr<ResponseObject>>>(
            batch_size, absl::StatusOr<ResponseObject>());
    auto finished_counter = std::make_shared<std::atomic<size_t>>(0);

    for (size_t index = 0; index < batch_size; ++index) {
      auto callback =
          [batch_response, finished_counter, batch_callback, index](
              std::unique_ptr<absl::StatusOr<ResponseObject>> obj_response) {
            batch_response->at(index) = *obj_response;
            auto finished_value = finished_counter->fetch_add(1);
            if (finished_value + 1 == batch_response->size()) {
              batch_callback(*batch_response);
            }
          };

      auto request = std::make_unique<RequestT>(batch[index]);
      core::ExecutionResult result;
      while ((result = Dispatcher::Dispatch(std::move(request), callback)) !=
             core::SuccessExecutionResult()) {
        // If the first request from the batch got a failure, return failure
        // without waiting.
        if (index == 0) {
          return result;
        }
        request = std::make_unique<RequestT>(batch[index]);
      }
    }

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Execute a "load" request against all worker in the pool.
   *
   * @param code_object The code object to load.
   * @param broadcast_callback The callback to invoke once the opeartion has
   * completed.
   * @return core::ExecutionResult Whether the broadcast succeeded or failed.
   */
  core::ExecutionResult Broadcast(std::unique_ptr<CodeObject> code_object,
                                  Callback broadcast_callback) noexcept;

 private:
  /**
   * @brief The internal dispatch function which puts a request into a worker
   * queue.
   *
   * @tparam RequestT The request type.
   * @param request The request.
   * @param callback The callback to invoke once the operation finishes.
   * @return core::ExecutionResult Whether the dispatch call succeeded or
   * failed.
   */
  template <typename RequestT>
  core::ExecutionResult InternalDispatch(std::unique_ptr<RequestT> request,
                                         Callback callback,
                                         int32_t worker_index = -1) noexcept {
    if (pending_requests_.load() >= max_pending_requests_) {
      return core::FailureExecutionResult(
          core::errors::SC_ROMA_DISPATCHER_DISPATCH_DISALLOWED_DUE_TO_CAPACITY);
    }

    // We accept empty request IDs, but we will replace them with a placeholder.
    if (request->id.empty()) {
      request->id = constants::kDefaultRomaRequestId;
    }

    auto validation_result =
        request_validator::RequestValidator<RequestT>::Validate(request);
    if (!validation_result.Successful()) {
      return validation_result;
    }

    size_t index = 0;

    if (worker_index != -1) {
      index = worker_index;
    } else {
      auto num_workers = worker_pool_->GetPoolSize();
      index = worker_index_.fetch_add(1) % num_workers;
      worker_index_ = worker_index_.load() % num_workers;
    }

    if constexpr (std::is_same<RequestT, CodeObject>::value) {
      code_object_cache_.Set(request->version_num, *request);
    }

    const auto& request_id = request->id;
    // This is a workaround to be able to register a lambda that needs to
    // capture a non-copy-constructible input (request)
    auto shared_request =
        std::make_shared<std::unique_ptr<RequestT>>(std::move(request));

    auto schedule_result = async_executor_->Schedule(
        [this, index, shared_request, callback] {
          auto request = std::move(*shared_request);
          std::unique_ptr<absl::StatusOr<ResponseObject>> response_or;

          auto worker_or = worker_pool_->GetWorker(index);
          if (!worker_or.result().Successful()) {
            response_or = std::make_unique<absl::StatusOr<ResponseObject>>(
                absl::Status(absl::StatusCode::kInternal,
                             core::errors::GetErrorMessage(
                                 worker_or.result().status_code)));
            callback(::std::move(response_or));
            pending_requests_--;
            return;
          }

          if (!code_object_cache_.Contains(request->version_num)) {
            response_or = std::make_unique<absl::StatusOr<ResponseObject>>(
                absl::Status(absl::StatusCode::kInternal,
                             "Could not find code version in cache."));
            callback(::std::move(response_or));
            pending_requests_--;
            return;
          }

          auto request_type =
              code_object_cache_.Get(request->version_num).js.empty()
                  ? constants::kRequestTypeWasm
                  : constants::kRequestTypeJavascript;
          if (!code_object_cache_.Get(request->version_num).wasm_bin.empty()) {
            request_type = constants::kRequestTypeJavascriptWithWasm;
          }

          auto run_code_request_or =
              request_converter::RequestConverter<RequestT>::FromUserProvided(
                  request, request_type);
          if (!run_code_request_or.result().Successful()) {
            response_or = std::make_unique<absl::StatusOr<ResponseObject>>(
                absl::Status(absl::StatusCode::kInternal,
                             core::errors::GetErrorMessage(
                                 run_code_request_or.result().status_code)));
            callback(::std::move(response_or));
            pending_requests_--;
            return;
          }

          auto run_code_response_or =
              (*worker_or)->RunCode(*run_code_request_or);
          if (!run_code_response_or.result().Successful()) {
            auto err_msg = core::errors::GetErrorMessage(
                run_code_response_or.result().status_code);
            response_or = std::make_unique<absl::StatusOr<ResponseObject>>(
                absl::Status(absl::StatusCode::kInternal, err_msg));
            LOG(ERROR) << "The worker " << index
                       << " execute the request failed due to " << err_msg;

            if (run_code_response_or.result().Retryable()) {
              // This means that the worker crashed and the request could be
              // retried, however, we need to reload the worker with the
              // cached code.
              auto reload_result = ReloadCachedCodeObjects(*worker_or);
              if (!reload_result.Successful()) {
                LOG(ERROR) << "Reloading the worker cache failed with "
                           << core::errors::GetErrorMessage(
                                  reload_result.status_code);
              }
              ROMA_VLOG(1)
                  << "Successfully reload all cached code objects to the worker"
                  << index;
            }

            callback(::std::move(response_or));
            pending_requests_--;
            return;
          }

          ResponseObject response_object;
          response_or =
              std::make_unique<absl::StatusOr<ResponseObject>>(response_object);
          response_or->value().id = request->id;
          response_or->value().resp = move(*run_code_response_or->response);
          for (auto& kv : run_code_response_or->metrics) {
            response_or->value().metrics[kv.first] = kv.second;
          }
          callback(::std::move(response_or));
          pending_requests_--;
        },
        core::AsyncPriority::Normal);

    if (schedule_result.Successful()) {
      ROMA_VLOG(1) << "Successfully schedule the execution for request "
                   << request_id << " in worker " << index;
      pending_requests_++;
    }

    return schedule_result;
  }

  core::ExecutionResult ReloadCachedCodeObjects(
      std::shared_ptr<worker_api::WorkerApi>& worker);

  std::shared_ptr<core::AsyncExecutor> async_executor_;
  std::shared_ptr<worker_pool::WorkerPool> worker_pool_;
  std::atomic<size_t> worker_index_;
  std::atomic<size_t> pending_requests_;
  const size_t max_pending_requests_;
  core::common::LruCache<uint64_t, CodeObject> code_object_cache_;
};
}  // namespace google::scp::roma::sandbox::dispatcher
