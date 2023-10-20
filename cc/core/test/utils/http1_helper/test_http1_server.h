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

#include <netinet/in.h>

#include <map>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include "core/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::test {

// Returns an unused TCP port number.
ExecutionResultOr<in_port_t> GetUnusedPortNumber();

// Lightweight Boost HTTP/1.1 server.
// After the constructor returns, the server is ready to accept incoming
// requests on GetPath().
class TestHttp1Server {
 public:
  // Run the mock server on a random unused port.
  TestHttp1Server();

  // Gets the port number the server is running on.
  in_port_t PortNumber() const;

  // Gets the full path to this server i.e. 'http://localhost:8080'
  std::string GetPath() const;

  // Returns the request object that this server most recently received.
  const boost::beast::http::request<boost::beast::http::dynamic_body>& Request()
      const;

  // Returns the most recently received request's body as a string.
  std::string RequestBody() const;

  // Sets the HTTP response status to return to clients - default is OK.
  void SetResponseStatus(boost::beast::http::status status);

  // Sets the HTTP response body to return to clients - default is empty.
  void SetResponseBody(const BytesBuffer& body);

  // Sets the headers to return in the HTTP response.
  void SetResponseHeaders(
      const std::multimap<std::string, std::string>& response_headers);

  ~TestHttp1Server();

 private:
  // Initiate the asynchronous operations associated with the connection.
  void ReadFromSocketAndWriteResponse(boost::asio::ip::tcp::socket& socket);

  // The most recent request which was processed by the server.
  boost::beast::http::request<boost::beast::http::dynamic_body> request_;

  // The status to return to the client.
  boost::beast::http::status response_status_ = boost::beast::http::status::ok;

  // The body to send in the HttpResponse.
  BytesBuffer response_body_;
  // A map of header names to values to send in the HttpResponse.
  std::multimap<std::string, std::string> response_headers_;

  // The thread which this server is running on.
  std::thread thread_;
  in_port_t port_number_ = 0;

  // Indicates when thread should exit (false).
  std::atomic_bool run_{true};
  // Indicates when a call to async_accept is outstanding.
  std::atomic_bool accepting_{false};
};

std::multimap<std::string, std::string> GetRequestHeadersMap(
    const boost::beast::http::request<boost::beast::http::dynamic_body>&
        request);

}  // namespace google::scp::core::test
