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

#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/generic/basic_endpoint.hpp>
#include <boost/asio/generic/stream_protocol.hpp>

namespace google::scp::proxy {
using Protocol = boost::asio::generic::stream_protocol;
using Socket = Protocol::socket;
using Acceptor = boost::asio::basic_socket_acceptor<Protocol>;
using Endpoint = boost::asio::generic::basic_endpoint<Protocol>;
using Executor = Socket::executor_type;
}  // namespace google::scp::proxy
