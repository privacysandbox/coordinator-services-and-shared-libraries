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

#include "core/http2_forwarder/src/http2_forwarder.h"

#include <memory>
#include <utility>

using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::placeholders::_1;

namespace google::scp::core {

Http2Forwarder::Http2Forwarder(
    const std::shared_ptr<HttpClientInterface>& http2_client)
    : http2_client_(http2_client) {}

ExecutionResult Http2Forwarder::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult Http2Forwarder::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult Http2Forwarder::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult Http2Forwarder::RouteRequest(
    AsyncContext<HttpRequest, HttpResponse>& original_context) noexcept {
  // Make a new context to send the request to the endpoint requested
  AsyncContext<HttpRequest, HttpResponse> context;
  // Use the same request object as the requesting context.
  context.parent_activity_id = original_context.activity_id;
  context.correlation_id = original_context.correlation_id;
  context.request = original_context.request;
  context.callback =
      bind(&Http2Forwarder::OnHttpResponse, this, original_context, _1);
  return http2_client_->PerformRequest(context);
}

void Http2Forwarder::OnHttpResponse(
    AsyncContext<HttpRequest, HttpResponse>& original_context,
    AsyncContext<HttpRequest, HttpResponse>& context) noexcept {
  if (!context.result.Successful()) {
    FinishContext(context.result, original_context);
    return;
  }
  original_context.result = context.result;
  // Copy the response.
  original_context.response = make_shared<HttpResponse>();
  original_context.response->headers = context.response->headers;
  original_context.response->body = context.response->body;
  original_context.response->code = context.response->code;

  FinishContext(original_context.result, original_context);
}

}  // namespace google::scp::core
