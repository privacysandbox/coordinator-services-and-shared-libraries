
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
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nghttp2/asio_http2_client.h>

#include "cc/core/interface/async_context.h"
#include "core/common/concurrent_map/src/concurrent_map.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"
#include "http_connection.h"

namespace google::scp::core {
/**
 * @brief Provides connection pool functionality. Once the object is created,
 * the caller can get a connection to the remote host by calling get connection.
 * The order of the connections is chosen in a round robin fashion.
 *
 */
class HttpConnectionPool : public ServiceInterface {
 protected:
  /**
   * @brief The http connection pool entry to be kept in the concurrent map of
   * the active connections.
   */
  struct HttpConnectionPoolEntry {
    HttpConnectionPoolEntry() : is_initialized(false), order_counter(0) {}

    /// The current cached connections.
    std::vector<std::shared_ptr<HttpConnection>> http_connections;
    /// Indicates whether the entry is initialized.
    std::atomic<bool> is_initialized;
    /// Is used to apply a round robin fashion selection of the connections.
    std::atomic<uint64_t> order_counter;
  };

 public:
  /**
   * @brief Constructs a new Http Connection Pool object
   *
   * @param async_executor An instance of the async executor.
   * @param max_connections_per_host The max number of connections created per
   * host.
   */
  explicit HttpConnectionPool(
      const std::shared_ptr<AsyncExecutorInterface>& async_executor,
      size_t max_connections_per_host = kDefaultMaxConnectionsPerHost,
      TimeDuration http2_read_timeout_in_sec =
          kDefaultHttp2ReadTimeoutInSeconds)
      : async_executor_(async_executor),
        max_connections_per_host_(max_connections_per_host),
        http2_read_timeout_in_sec_(http2_read_timeout_in_sec),
        is_running_(false) {}

  ExecutionResult Init() noexcept;
  ExecutionResult Run() noexcept;
  ExecutionResult Stop() noexcept;

  /**
   * @brief Gets a connection for the provided uri.
   *
   * @param uri The uri to create a http connection to.
   * @param connection The created/cached connection.
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult GetConnection(
      const std::shared_ptr<Uri>& uri,
      std::shared_ptr<HttpConnection>& connection) noexcept;

 protected:
  /**
   * @brief Create a Http Connection object
   *
   * @param host host name
   * @param service service name a.k.a. service port
   * @param is_https
   * @return shared_ptr<HttpConnection>
   */
  virtual std::shared_ptr<HttpConnection> CreateHttpConnection(
      std::string host, std::string service, bool is_https,
      TimeDuration http2_read_timeout_in_sec);

  /**
   * @brief If a connection goes bad for any reason, the connection pool will
   * recycle the connection by stopping it and reseting the object.
   *
   * @param connection The connection to be recycled.
   */
  virtual void RecycleConnection(
      std::shared_ptr<HttpConnection>& connection) noexcept;

  /// Instance of the async executor.
  const std::shared_ptr<AsyncExecutorInterface> async_executor_;

  /// Max number of connections per host.
  size_t max_connections_per_host_;

  /// http2 connection read timeout in seconds.
  TimeDuration http2_read_timeout_in_sec_;

  /// The pool of all the connections.
  core::common::ConcurrentMap<std::string,
                              std::shared_ptr<HttpConnectionPoolEntry>>
      connections_;

  /// Indicates whether the connection pool is running.
  std::atomic<bool> is_running_;
  /// Mutex for recycling connection
  std::mutex connection_lock_;
};
}  // namespace google::scp::core
