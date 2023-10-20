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

#include <stdint.h>

namespace google::scp::proxy::socket_vendor {

enum class MessageType : uint32_t {
  kBindRequest = 0u,
  kBindResponse,
  kListenRequest,
  kListenResponse,
  kNewConnectionResponse,
  // Anything not less than kInvalidMessage are invalid.
  kInvalidMessage,
};

struct Message {
  MessageType type;

  Message() : type(MessageType::kInvalidMessage) {}

  explicit Message(MessageType type) : type(type) {}
};

struct BindRequest : public Message {
  uint16_t port;  // port in host byte order

  BindRequest() : Message(MessageType::kBindRequest) {}
};

struct BindResponse : public Message {
  uint16_t port;  // Port in host byte order

  BindResponse() : Message(MessageType::kBindResponse) {}
};

struct ListenRequest : public Message {
  int32_t backlog;

  ListenRequest() : Message(MessageType::kListenRequest) {}
};

struct ListenResponse : public Message {
  ListenResponse() : Message(MessageType::kListenResponse) {}
};

struct NewConnectionResponse : public Message {
  uint8_t addr[16];
  uint16_t port;  // In network byte order

  NewConnectionResponse() : Message(MessageType::kNewConnectionResponse) {}
};

}  // namespace google::scp::proxy::socket_vendor
