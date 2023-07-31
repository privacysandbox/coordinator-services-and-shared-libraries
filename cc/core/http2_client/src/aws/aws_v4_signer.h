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
#include <string>
#include <vector>

#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core {
/**
 * @brief A simple implementation of AWS SigV4 signer.
 *
 */
class AwsV4Signer {
 public:
  AwsV4Signer(const std::string& aws_access_key,
              const std::string& aws_secret_key,
              const std::string& aws_security_token,
              const std::string& service_name, const std::string& aws_region);

  /**
   * @brief Sign the request based on \a headers_to_sign, which stores the
   * headers-to-sign as a vector of strings.
   *
   * @param[in,out] http_request The HttpRequest to sign.
   * @param[in] headers_to_sign The headers to include.
   * @return ExecutionResult SuccessExecutionResult if everything required are
   * valid. Otherwise failure.
   */
  ExecutionResult SignRequest(
      HttpRequest& http_request,
      std::vector<std::string>& headers_to_sign) noexcept;

  /**
   * @brief Sign the request based on \a headers_to_sign, which stores the
   * headers-to-sign as a ";" delimited string.
   *
   * @param[in,out] http_request The HttpRequest to sign.
   * @param[in] headers_to_sign The headers to include. ";, " delimited.
   * @return ExecutionResult SuccessExecutionResult if everything required are
   * valid. Otherwise failure.
   */
  ExecutionResult SignRequest(HttpRequest& http_request,
                              const std::string& headers_to_sign) noexcept;

  /**
   * @brief Sign the request based on \a header_begin and \a header_end, which
   * are iterators to arbitrary container of headers to sign.
   *
   * @param[in,out] http_request The HttpRequest to sign.
   * @param[in] header_begin The first header to include.
   * @param[in] header_end The iterator past last header to include.
   * @return ExecutionResult SuccessExecutionResult if everything required are
   * valid. Otherwise failure.
   */
  template <class HeadersIter>
  ExecutionResult SignRequest(HttpRequest& http_request,
                              HeadersIter header_begin,
                              HeadersIter header_end) noexcept {
    std::vector<std::string> headers(header_begin, header_end);
    return SignRequest(http_request, headers);
  }

  /**
   * @brief Sign the request based on a pre-calculated signature. This method
   * does not require valid credentials and hence no validation of the signature
   * is done locally.
   *
   * @param[in,out] http_request The HttpRequest to sign.
   * @param[in] headers_to_sign The headers to include.
   * @param signature The pre-calculated signature.
   * @return ExecutionResult
   */
  ExecutionResult SignRequestWithSignature(
      HttpRequest& http_request, std::vector<std::string>& headers_to_sign,
      const std::string& x_amz_date, const std::string& signature) noexcept;

  /**
   * @brief Calculate the signature of a request. Return the parts useful for
   * signing or pre-signing.
   *
   * @param[in,out] http_request The HttpRequest to sign. This function may add
   * the x-amz-date header to it if none exists.
   * @param signature[out] The calculated signature in hex string form.
   * @param x_amz_date[out] The date string, i.e. the x-amz-date header value.
   * @return ExecutionResult
   */
  ExecutionResult GetSignatureParts(HttpRequest& http_request,
                                    std::vector<std::string>& headers_to_sign,
                                    std::string& signature,
                                    std::string& x_amz_date) noexcept;

  /**
   * @brief Create Canonical Request from the http_request.
   *
   * @param[out] canonical_request The canonical request created.
   * @param[in] http_request The HTTP request to create canonical request from.
   * @return ExecutionResult
   */
  ExecutionResult CreateCanonicalRequest(
      std::string& canonical_request, HttpRequest& http_request,
      const std::vector<std::string>& headers_to_sign) noexcept;

  const std::string& GetAwsAccessKey() { return aws_access_key_; }

  const std::string& GetAwsSecurityToken() { return aws_security_token_; }

  const std::string& GetRegion() { return aws_region_; }

 private:
  /**
   * @brief Get the timestamp string at the time of calling, in AWS required
   * format.
   *
   * @return std::string The timestamp string.
   */
  static std::string GetSigningTime();

  static inline std::string DateFromTimestamp(const std::string x_amz_date) {
    constexpr size_t date_len = sizeof("YYYYMMdd") - 1;
    return x_amz_date.substr(0, date_len);
  }

  /**
   * @brief Calculate the signature of string_to_sign.
   *
   * @return std::string The signature in hex string form.
   */
  std::string CalculateSignature(const std::string& string_to_sign,
                                 const std::string& date) noexcept;

  /**
   * @brief Produce a signature header and add to the request.
   *
   * @param[in] headers_to_sign The pre-sorted headers to sign.
   * @param signature  The signature in hex.
   */
  void AddSignatureHeader(HttpRequest& http_request,
                          const std::vector<std::string>& headers_to_sign,
                          const std::string& date,
                          const std::string& signature);

  const std::string aws_access_key_;
  const std::string aws_secret_key_;
  const std::string aws_security_token_;
  const std::string service_name_;
  const std::string aws_region_;
  /// The list of of header names to produce "signed headers", sorted by their
  /// lower case order.
  std::vector<std::string> headers_to_sign_;
};
}  // namespace google::scp::core
