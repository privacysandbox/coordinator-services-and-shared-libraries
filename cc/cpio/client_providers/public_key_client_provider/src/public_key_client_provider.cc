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

#include "public_key_client_provider.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/async_context.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/http_types.h"
#include "cpio/client_providers/interface/public_key_client_provider_interface.h"
#include "cpio/client_providers/interface/type_def.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/public_key_client/type_def.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

#include "error_codes.h"
#include "public_key_client_utils.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::protobuf::Any;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_HTTP_CLIENT_REQUIRED;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_INVALID_CONFIG_OPTIONS;
using std::atomic;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::vector;
using std::placeholders::_1;

static constexpr int kSToMsConversionBase = 1e3;
static constexpr char kPublicKeyClientProvider[] = "PublicKeyClientProvider";

namespace google::scp::cpio::client_providers {

ExecutionResult PublicKeyClientProvider::Init() noexcept {
  if (!public_key_client_options_ ||
      !public_key_client_options_->endpoints.size()) {
    auto execution_result = FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_INVALID_CONFIG_OPTIONS);
    SCP_ERROR(kPublicKeyClientProvider, kZeroUuid, execution_result,
              "Failed to init PublicKeyClientProvider.");
    return execution_result;
  }

  if (!http_client_) {
    auto execution_result = FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_HTTP_CLIENT_REQUIRED);
    SCP_ERROR(kPublicKeyClientProvider, kZeroUuid, execution_result,
              "Failed to init PublicKeyClientProvider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

void PublicKeyClientProvider::OnListPublicKeys(
    AsyncContext<Any, Any> any_context) noexcept {
  auto request = make_shared<ListPublicKeysRequest>();
  any_context.request->UnpackTo(request.get());
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      move(request),
      bind(CallbackToPackAnyResponse<ListPublicKeysRequest,
                                     ListPublicKeysResponse>,
           any_context, _1),
      any_context);
  context.result = ListPublicKeys(context);
}

ExecutionResult PublicKeyClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PublicKeyClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PublicKeyClientProvider::ListPublicKeys(
    AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse>&
        public_key_fetching_context) noexcept {
  // Use got_success_result and unfinished_counter to track whether get success
  // response and how many failed responses. Only return one response whether
  // success or failed.
  auto got_success_result = make_shared<atomic<bool>>(false);
  auto unfinished_counter =
      make_shared<atomic<size_t>>(public_key_client_options_->endpoints.size());

  ExecutionResult result = FailureExecutionResult(
      SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED);
  for (auto uri : public_key_client_options_->endpoints) {
    auto shared_uri = make_shared<Uri>(uri);

    auto http_request = make_shared<HttpRequest>();
    http_request->method = HttpMethod::GET;
    http_request->path = move(shared_uri);

    AsyncContext<HttpRequest, HttpResponse> http_client_context(
        move(http_request),
        bind(&PublicKeyClientProvider::OnPerformRequestCallback, this,
             public_key_fetching_context, _1, got_success_result,
             unfinished_counter),
        public_key_fetching_context);

    auto execution_result = http_client_->PerformRequest(http_client_context);
    if (execution_result.Successful()) {
      // If there is one URI PerformRequest() success, ListPublicKeys will
      // return success.
      result = SuccessExecutionResult();
    } else {
      SCP_ERROR_CONTEXT(kPublicKeyClientProvider, public_key_fetching_context,
                        execution_result,
                        "Failed to perform request with endpoint %s.",
                        uri.c_str());
    }
  }

  if (!result.Successful()) {
    public_key_fetching_context.result = result;
    SCP_ERROR_CONTEXT(kPublicKeyClientProvider, public_key_fetching_context,
                      public_key_fetching_context.result,
                      "Failed to perform request with config endpoints.");
    public_key_fetching_context.Finish();
  }

  return result;
}

void ExecutionResultCheckingHelper(
    AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse>& context,
    const ExecutionResult& result,
    shared_ptr<atomic<size_t>> unfinished_counter) noexcept {
  auto pervious_unfinished = unfinished_counter->fetch_sub(1);
  if (pervious_unfinished == 1) {
    context.result = result;
    SCP_ERROR_CONTEXT(kPublicKeyClientProvider, context, context.result,
                      "Failed to fetch public keys.");
    context.Finish();
  }
}

void PublicKeyClientProvider::OnPerformRequestCallback(
    AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse>&
        public_key_fetching_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context,
    shared_ptr<atomic<bool>> got_success_result,
    shared_ptr<atomic<size_t>> unfinished_counter) noexcept {
  if (got_success_result->load()) {
    return;
  }

  auto result = http_client_context.result;
  if (!result.Successful()) {
    return ExecutionResultCheckingHelper(public_key_fetching_context, result,
                                         unfinished_counter);
  }

  uint64_t expired_time_in_s;
  auto headers = *http_client_context.response->headers;
  result = PublicKeyClientUtils::ParseExpiredTimeFromHeaders(headers,
                                                             expired_time_in_s);
  if (!result.Successful()) {
    return ExecutionResultCheckingHelper(public_key_fetching_context, result,
                                         unfinished_counter);
  }

  vector<PublicKey> public_keys;
  auto body = http_client_context.response->body;
  result = PublicKeyClientUtils::ParsePublicKeysFromBody(body, public_keys);
  if (!result.Successful()) {
    return ExecutionResultCheckingHelper(public_key_fetching_context, result,
                                         unfinished_counter);
  }

  auto got_result = false;
  if (got_success_result->compare_exchange_strong(got_result, true)) {
    public_key_fetching_context.response =
        make_shared<ListPublicKeysResponse>();
    *public_key_fetching_context.response->mutable_expiration_time() =
        TimeUtil::SecondsToTimestamp(expired_time_in_s);
    public_key_fetching_context.response->mutable_public_keys()->Add(
        public_keys.begin(), public_keys.end());
    public_key_fetching_context.result = SuccessExecutionResult();
    public_key_fetching_context.Finish();
  }
}

shared_ptr<PublicKeyClientProviderInterface>
PublicKeyClientProviderFactory::Create(
    const shared_ptr<PublicKeyClientOptions>& options,
    const shared_ptr<HttpClientInterface>& http_client) {
  return make_shared<PublicKeyClientProvider>(options, http_client);
}

}  // namespace google::scp::cpio::client_providers
