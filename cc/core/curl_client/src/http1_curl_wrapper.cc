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
#include "http1_curl_wrapper.h"

#include <algorithm>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/utils/src/http.h"

#include "error_codes.h"

using google::scp::core::common::kZeroUuid;
using google::scp::core::utils::GetEscapedUriWithQuery;
using std::make_shared;
using std::make_unique;
using std::move;
using std::regex;
using std::regex_search;
using std::shared_ptr;
using std::smatch;
using std::stoi;
using std::string;
using std::unique_ptr;
using std::vector;

namespace google::scp::core {

namespace {

constexpr int64_t kTrueAsLong = 1L;
constexpr int64_t kCurlOptTimeout = 60L;
constexpr char kHttp1CurlWrapper[] = "Http1CurlWrapper";

ExecutionResult GetExecutionResultFromCurlError(const string& err_buffer) {
  regex error_code_regex("([0-9]{3})");
  smatch http_code_match;
  int http_code;
  if (regex_search(err_buffer, http_code_match, error_code_regex)) {
    try {
      http_code = stoi(http_code_match.str(0));
    } catch (...) {
      auto result = RetryExecutionResult(
          errors::SC_CURL_CLIENT_REQUEST_BAD_REGEX_PARSING);
      SCP_ERROR(kHttp1CurlWrapper, kZeroUuid, result,
                "Could not parse HTTP status code to integer: %s",
                http_code_match.str(0).c_str());
      return result;
    }
  } else {
    auto result =
        RetryExecutionResult(errors::SC_CURL_CLIENT_REQUEST_BAD_REGEX_PARSING);
    SCP_ERROR(kHttp1CurlWrapper, kZeroUuid, result,
              "Could not find HTTP status in HTTP error message: %s",
              err_buffer.c_str());
    return result;
  }
  if (http_code < 400) {
    return SuccessExecutionResult();
  }
  switch (http_code) {
    case 400:
      return FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_FAILED);
    case 401:
      return FailureExecutionResult(
          errors::SC_CURL_CLIENT_REQUEST_UNAUTHORIZED);
    case 403:
      return FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_FORBIDDEN);
    case 404:
      return FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_NOT_FOUND);
    case 409:
      return FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_CONFLICT);
    case 500:
      return RetryExecutionResult(errors::SC_CURL_CLIENT_REQUEST_SERVER_ERROR);
    case 501:
      return RetryExecutionResult(
          errors::SC_CURL_CLIENT_REQUEST_NOT_IMPLEMENTED);
    case 503:
      return RetryExecutionResult(
          errors::SC_CURL_CLIENT_REQUEST_SERVICE_UNAVAILABLE);
    default:
      return RetryExecutionResult(
          errors::SC_CURL_CLIENT_REQUEST_OTHER_HTTP_ERROR);
  }
}

/**
 * @brief Interprets contents as a char* of length byte_size * num_bytes and
 * writes them into output which should be a BytesBuffer*
 *
 * https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 * @param contents The contents to write to the output
 * @param byte_size The size of each member (char in this case; always 1)
 * @param num_bytes How many members (chars) are in contents
 * @param output A BytesBuffer* to write contents into
 * @return size_t The amount of data written
 */
size_t ResponsePayloadHandler(char* contents, size_t byte_size,
                              size_t num_bytes, void* output) {
  BytesBuffer* output_buffer = static_cast<BytesBuffer*>(output);
  size_t contents_length = byte_size * num_bytes;
  output_buffer->bytes = make_shared<vector<Byte>>(contents_length);
  for (size_t i = 0; i < contents_length; i++) {
    output_buffer->bytes->at(i) = contents[i];
  }
  output_buffer->length = contents_length;
  output_buffer->capacity = contents_length;
  return contents_length;
}

/**
 * @brief Interprets output as a HttpHeaders*. Parses contents into a
 * colon-separated header string and stores the key-value pair in output.
 * This is called for each header individually - including the blank line
 * header.
 *
 * https://curl.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
 *
 * @param contents A header acquired from the response - not null terminated
 * @param byte_size The size of each member (char in this case; always 1)
 * @param num_bytes How many members (chars) are in contents
 * @param output The header map to place this header into.
 * @return size_t The amount of characters processed.
 */
size_t ResponseHeaderHandler(char* contents, size_t byte_size, size_t num_bytes,
                             void* output) {
  HttpHeaders* header_map = static_cast<HttpHeaders*>(output);
  size_t contents_size = byte_size * num_bytes;
  if (contents_size <= 2) {
    // Empty field line (i.e. "\r\n") - skip.
    return contents_size;
  }
  string contents_str(contents, contents_size);
  if (regex r("HTTP.*[0-9]{3}"); regex_search(contents_str, r)) {
    // The header is just the HTTP response code.
    return contents_size;
  }

  // The index of the carriage return character '\r'.
  size_t contents_end = contents_str.find('\r');
  // The index of the colon character ':'.
  size_t colon_index = contents_str.find(':');

  if (colon_index > contents_end) {
    SCP_ERROR(
        kHttp1CurlWrapper, kZeroUuid,
        FailureExecutionResult(errors::SC_CURL_CLIENT_BAD_HEADER_RECEIVED),
        "The ':' was found after the '\r' in the header: \"%s\"",
        contents_str.c_str());
    return contents_size;
  }
  if (colon_index == string::npos) {
    SCP_ERROR(
        kHttp1CurlWrapper, kZeroUuid,
        FailureExecutionResult(errors::SC_CURL_CLIENT_BAD_HEADER_RECEIVED),
        "No ':' was found in the header: \"%s\"", contents_str.c_str());
    return contents_size;
  }
  bool has_space_after_colon = contents_str[colon_index + 1] == ' ';

  // Copy the position after the colon until the end.
  size_t value_index = colon_index + 1;
  if (has_space_after_colon) value_index++;

  header_map->insert(
      {contents_str.substr(0, colon_index),
       contents_str.substr(value_index, contents_end - value_index)});
  return contents_size;
}

/**
 * @brief Read the request userdata to contents.
 *
 * https://curl.se/libcurl/c/CURLOPT_READFUNCTION.html
 *
 * @param contents The output array to copy userdata into.
 * @param byte_size The size of each member (char in this case; always 1)
 * @param num_bytes How many members (chars) are in contents
 * @param userdata BytesBuffer* of the data to copy into contents
 * @return size_t The amount of characters processed.
 */
size_t RequestReadHandler(char* contents, size_t byte_size, size_t num_bytes,
                          void* userdata) {
  BytesBuffer* input_buffer = static_cast<BytesBuffer*>(userdata);

  int64_t bytes_to_read = byte_size * num_bytes;
  if (bytes_to_read > input_buffer->length) {
    bytes_to_read = input_buffer->length;
  }

  if (bytes_to_read) {
    memcpy(contents, input_buffer->bytes->data(), bytes_to_read);
  }
  return bytes_to_read;
}

}  // namespace

ExecutionResultOr<shared_ptr<Http1CurlWrapper>>
Http1CurlWrapper::MakeWrapper() {
  CURL* curl = curl_easy_init();
  if (!curl) {
    auto result = RetryExecutionResult(errors::SC_CURL_CLIENT_CURL_INIT_ERROR);
    SCP_ERROR(kHttp1CurlWrapper, kZeroUuid, result, "failed to make wrapper");
    return result;
  }
  return make_shared<Http1CurlWrapper>(curl);
}

ExecutionResultOr<unique_ptr<curl_slist, CurlListDeleter>>
Http1CurlWrapper::AddHeadersToRequest(
    const std::shared_ptr<HttpHeaders>& headers) {
  if (!headers || headers->empty()) {
    return SuccessExecutionResult();
  }
  curl_slist* header_list = nullptr;
  for (const auto& [key, value] : *headers) {
    string header = absl::StrCat(key, ": ", value);
    auto* result = curl_slist_append(header_list, header.c_str());
    if (result == nullptr) {
      curl_slist_free_all(header_list);
      return RetryExecutionResult(errors::SC_CURL_CLIENT_CURL_HEADER_ADD_ERROR);
    }
    header_list = result;
  }
  curl_easy_setopt(curl_.get(), CURLOPT_HTTPHEADER, header_list);
  // Wrap the returned raw pointer in a unique_ptr to prevent memory leaks.
  return unique_ptr<curl_slist, CurlListDeleter>(header_list);
}

void Http1CurlWrapper::SetUpResponseHeaderHandler(
    HttpHeaders* returned_header_destination) {
  curl_easy_setopt(curl_.get(), CURLOPT_HEADERFUNCTION, ResponseHeaderHandler);
  curl_easy_setopt(curl_.get(), CURLOPT_HEADERDATA,
                   returned_header_destination);
}

void Http1CurlWrapper::SetUpPostData(const BytesBuffer& body) {
  if (body.length == 0) {
    return;
  }
  curl_easy_setopt(curl_.get(), CURLOPT_POSTFIELDS, body.bytes->data());
  // This method of upload supports up to 2GB upload data.
  // See https://curl.se/libcurl/c/CURLOPT_POSTFIELDSIZE_LARGE.html for larger
  // uploads.
  curl_easy_setopt(curl_.get(), CURLOPT_POSTFIELDSIZE, body.length);
}

void Http1CurlWrapper::SetUpPutData(const BytesBuffer& body) {
  curl_easy_setopt(curl_.get(), CURLOPT_READFUNCTION, RequestReadHandler);

  curl_easy_setopt(curl_.get(), CURLOPT_READDATA, &body);

  curl_easy_setopt(curl_.get(), CURLOPT_INFILESIZE_LARGE, body.length);
}

// Performs the request. Logs any error that occurs and returns the status
// of the request. If the request was successful, response_ will now hold the
// body of the response.
ExecutionResultOr<HttpResponse> Http1CurlWrapper::PerformRequest(
    const HttpRequest& request) {
  if (!request.path || request.path->empty()) {
    return FailureExecutionResult(errors::SC_CURL_CLIENT_NO_PATH_SUPPLIED);
  }
  CURLoption option;
  switch (request.method) {
    case HttpMethod::GET:
      option = CURLOPT_HTTPGET;
      break;
    case HttpMethod::POST:
      option = CURLOPT_POST;
      SetUpPostData(request.body);
      break;
    case HttpMethod::PUT:
      option = CURLOPT_UPLOAD;
      SetUpPutData(request.body);
      break;
    case HttpMethod::UNKNOWN:
    default:
      return FailureExecutionResult(errors::SC_CURL_CLIENT_UNSUPPORTED_METHOD);
  }
  curl_easy_setopt(curl_.get(), option, kTrueAsLong);

  auto header_list = AddHeadersToRequest(request.headers);
  RETURN_IF_FAILURE(header_list.result());
  // Build the URL with the escaped path.
  auto uri = GetEscapedUriWithQuery(request);
  RETURN_IF_FAILURE(uri.result());

  curl_easy_setopt(curl_.get(), CURLOPT_URL, uri->c_str());

  HttpResponse response;
  response.headers = make_shared<HttpHeaders>();
  SetUpResponseHeaderHandler(response.headers.get());

  // Add the handler indicating what to do with the returned HTTP response.
  curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION, ResponsePayloadHandler);
  curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl_.get(), CURLOPT_TIMEOUT, kCurlOptTimeout);
  curl_easy_setopt(curl_.get(), CURLOPT_FAILONERROR, kTrueAsLong);
  // Create a buffer to place any error messages in.
  string err_str(CURL_ERROR_SIZE, '\0');
  curl_easy_setopt(curl_.get(), CURLOPT_ERRORBUFFER, err_str.data());

  // Execute the request.
  CURLcode perform_res = curl_easy_perform(curl_.get());
  if (perform_res != CURLE_OK) {
    auto result = GetExecutionResultFromCurlError(err_str);
    if (err_str.empty()) err_str = "<empty>";
    SCP_ERROR(kHttp1CurlWrapper, kZeroUuid, result,
              "CURL HTTP request failed with error code: %s, message: %s",
              curl_easy_strerror(perform_res), err_str.c_str());
    return result;
  }
  response.code = errors::HttpStatusCode::OK;
  return response;
}

Http1CurlWrapper::Http1CurlWrapper(CURL* curl) {
  // Wrap the returned raw pointer in a unique_ptr to prevent memory leaks.
  curl_ = unique_ptr<CURL, CurlHandleDeleter>(curl);
}

ExecutionResultOr<shared_ptr<Http1CurlWrapper>>
Http1CurlWrapperProvider::MakeWrapper() {
  return Http1CurlWrapper::MakeWrapper();
}

}  // namespace google::scp::core
