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
#include <thread>

#include <nghttp2/asio_http2_client.h>

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/http_client_interface.h"
#include "core/common/concurrent_map/src/concurrent_map.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core {
/**
 * @brief HttpConnection uses nghttp2 to establish http2 connections with the
 * remote hosts.
 */
class HttpConnection : public ServiceInterface {
 public:
  /**
   * @brief Constructs a new Http Connection object
   *
   * @param async_executor An instance of the async executor.
   * @param host The remote host for creating the connection.
   * @param service The port of the connection.
   * @param is_https If the connection is https, must be set to true.
   * @param http2_read_timeout_in_sec nghttp2 read timeout in second.
   */
  HttpConnection(const std::shared_ptr<AsyncExecutorInterface>& async_executor,
                 const std::string& host, const std::string& service,
                 bool is_https,
                 TimeDuration http2_read_timeout_in_sec =
                     kDefaultHttp2ReadTimeoutInSeconds);

  ExecutionResult Init() noexcept override;
  ExecutionResult Run() noexcept override;
  ExecutionResult Stop() noexcept override;

  /**
   * @brief Executes the http request and processes the response.
   *
   * @param http_context The context of the http operation.
   * @return ExecutionResult The execution result of the operation.
   */
  ExecutionResult Execute(
      AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept;

  /**
   * @brief Indicates whether the connection to the remote server is dropped.
   *
   * @return true Connection not available.
   * @return false Connection is available.
   */
  bool IsDropped() noexcept;

  /**
   * @brief Indicates whether the connection to the remote server is ready for
   * outgoing requests.
   *
   * @return true Connection not ready.
   * @return false Connection is ready.
   */
  bool IsReady() noexcept;

  /**
   * @brief Resets the state of the connection.
   */
  void Reset() noexcept;

 protected:
  /**
   * @brief Executes the http requests and sends it over the wire.
   *
   * @param request_id The id of the request.
   * @param http_context The http context of the operation.
   */
  void SendHttpRequest(
      common::Uuid& request_id,
      AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept;

  /**
   * @brief Is called when the request/response stream is closed either
   * peacefully or with error.
   *
   * @param request_id The pending call request id to be used to remove the
   * element from the map.
   * @param http_context The http context of the operation.
   * @param error_code The error code of the stream closure operation.
   */
  void OnRequestResponseClosed(
      common::Uuid& request_id,
      AsyncContext<HttpRequest, HttpResponse>& http_context,
      uint32_t error_code) noexcept;

  /**
   * @brief Is called when the response is available to the request issuer.
   *
   * @param http_context The http context of the operation.
   * @param http_response The http response object.
   */
  void OnResponseCallback(
      AsyncContext<HttpRequest, HttpResponse>& http_context,
      const nghttp2::asio_http2::client::response& http_response) noexcept;

  /**
   * @brief Is called when the body of the stream is available to be read.
   *
   * @param http_context The http context of the operation.
   * @param data A chunk of response body data.
   * @param chunk_length The current chunk length.
   */
  void OnResponseBodyCallback(
      AsyncContext<HttpRequest, HttpResponse>& http_context,
      const uint8_t* data, size_t chunk_length) noexcept;

  /**
   * @brief Is called when the connection to the remote host is established.
   */
  void OnConnectionCreated(boost::asio::ip::tcp::resolver::iterator) noexcept;

  /**
   * @brief Is called when the connection to the remote host is dropped.
   */
  void OnConnectionError() noexcept;

  /**
   * @brief Cancels all the pending callbacks. This is used during connection
   * drop or stop.
   */
  void CancelPendingCallbacks() noexcept;

  /**
   * @brief Converts an http status code to execution result.
   *
   * @param http_status_code The http status code.
   * @return ExecutionResult The execution result.
   */
  ExecutionResult ConvertHttpStatusCodeToExecutionResult(
      const errors::HttpStatusCode http_status_code) noexcept;

  /// An instance of the async executor.
  const std::shared_ptr<AsyncExecutorInterface> async_executor_;
  /// The remote host to establish a connection.
  std::string host_;
  /// Indicates the port for connection.
  std::string service_;
  /// True if the scheme is https.
  bool is_https_;

  /// http2 read timeout in seconds.
  TimeDuration http2_read_timeout_in_sec_;
  /// The asio io_service to provide http functionality.
  std::unique_ptr<boost::asio::io_service> io_service_;
  /// The worker guard to run the io_service_.
  std::unique_ptr<
      boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>
      work_guard_;
  /// The worker thread to run the io_service_.
  std::shared_ptr<std::thread> worker_;
  /// An instance of the session.
  std::shared_ptr<nghttp2::asio_http2::client::session> session_;
  /// The tls configuration.
  boost::asio::ssl::context tls_context_;
  /// Indicates if the connection is ready to be used.
  std::atomic<bool> is_ready_;
  /// Indicates if the connection is dropped.
  std::atomic<bool> is_dropped_;
  common::ConcurrentMap<common::Uuid, AsyncContext<HttpRequest, HttpResponse>,
                        common::UuidCompare>
      pending_network_calls_;
};
}  // namespace google::scp::core
