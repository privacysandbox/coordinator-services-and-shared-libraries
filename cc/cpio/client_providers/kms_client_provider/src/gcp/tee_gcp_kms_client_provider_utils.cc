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

#include "tee_gcp_kms_client_provider_utils.h"

#include <nlohmann/json.hpp>

#include "absl/strings/str_format.h"

using std::string;

namespace {

constexpr char kAudience[] = "//iam.googleapis.com/%s";
constexpr char kImpersonationUrl[] =
    "https://iamcredentials.googleapis.com/v1/projects/-/serviceAccounts/"
    "%s:generateAccessToken";
}  // namespace

namespace google::scp::cpio::client_providers {
void TeeGcpKmsClientProviderUtils::CreateAttestedCredentials(
    const string& wip_provider, const string& service_account_to_impersonate,
    string& credential_json) noexcept {
  const nlohmann::json configuration{
      {"type", "external_account"},
      {"audience", absl::StrFormat(kAudience, wip_provider)},
      {"subject_token_type", "urn:ietf:params:oauth:token-type:jwt"},
      {"token_url", "https://sts.googleapis.com/v1/token"},
      {"service_account_impersonation_url",
       absl::StrFormat(kImpersonationUrl, service_account_to_impersonate)},
      {"credential_source",
       nlohmann::json{
           {"file",
            "/run/container_launcher/attestation_verifier_claims_token"}}},
  };

  credential_json = configuration.dump();
}
}  // namespace google::scp::cpio::client_providers
