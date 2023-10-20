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

#include "aws_authorizer_client_signer.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <nghttp2/asio_http2_client.h>
#include <openssl/base64.h>

#include "core/interface/authorization_service_interface.h"

#include "error_codes.h"

using boost::system::error_code;
using nghttp2::asio_http2::host_service_from_uri;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::vector;

static constexpr char kServiceName[] = "execute-api";

// TODO: move this to common place
static string Base64Encode(const string& raw_data) {
  size_t required_len = 0;
  if (EVP_EncodedLength(&required_len, raw_data.length()) == 0) {
    return string();
  }
  string out(required_len, '\0');

  size_t output_len = EVP_EncodeBlock(
      reinterpret_cast<uint8_t*>(out.data()),
      reinterpret_cast<const uint8_t*>(raw_data.data()), raw_data.length());
  if (output_len == 0) {
    return string();
  }
  out.pop_back();  // EVP_EncodeBlock writes trailing \0. pop it.
  return out;
}

namespace google::scp::core {
AwsAuthorizerClientSigner::AwsAuthorizerClientSigner(
    const string& aws_access_key, const string& aws_secret_key,
    const string& aws_security_token, const string& aws_region)
    : aws_v4_signer_(aws_access_key, aws_secret_key, aws_security_token,
                     kServiceName, aws_region) {}

ExecutionResult AwsAuthorizerClientSigner::GetAuthToken(const string& endpoint,
                                                        string& token) {
  vector<string> headers_to_sign{"Host", "X-Amz-Date"};
  // Since the signature is destined for a different HOST in our authorization
  // scheme, we create a temporary request just for generating the signature.
  HttpRequest tmp_request;
  tmp_request.path = make_shared<Uri>();
  tmp_request.path->assign(endpoint);
  tmp_request.method = HttpMethod::POST;
  tmp_request.headers = make_shared<HttpHeaders>();

  error_code http2_error_code;
  string scheme;
  string host;
  string service;
  if (host_service_from_uri(http2_error_code, scheme, host, service,
                            *tmp_request.path)) {
    return FailureExecutionResult(
        errors::SC_AUTHORIZATION_SERVICE_INVALID_CONFIG);
  }

  tmp_request.headers->insert({string("Host"), host});
  string signature;
  string x_amz_date;
  auto ret = aws_v4_signer_.GetSignatureParts(tmp_request, headers_to_sign,
                                              signature, x_amz_date);
  if (!ret) {
    return ret;
  }
  stringstream json_token_builder;
  json_token_builder << "{\"access_key\":\"" << aws_v4_signer_.GetAwsAccessKey()
                     << "\", \"signature\":\"" << signature
                     << "\", \"amz_date\":\"" << x_amz_date << "\"";
  if (aws_v4_signer_.GetAwsSecurityToken().length() > 0) {
    json_token_builder << ",\"security_token\":\""
                       << aws_v4_signer_.GetAwsSecurityToken() << "\"";
  }
  json_token_builder << '}';
  string json_token = json_token_builder.str();
  token = Base64Encode(json_token);
  return SuccessExecutionResult();
}

ExecutionResult AwsAuthorizerClientSigner::SignRequest(
    HttpRequest& http_request, const string& endpoint) {
  string token;
  auto ret = GetAuthToken(endpoint, token);
  if (!ret) {
    return ret;
  }
  http_request.headers->insert({string(kAuthHeader), token});
  return SuccessExecutionResult();
}

}  // namespace google::scp::core
