// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cpio/client_providers/kms_client_provider/src/gcp/tee_gcp_kms_client_provider_utils.h"

#include <gtest/gtest.h>

#include <string>

using std::string;

static constexpr char kWipProvider[] = "wip";
static constexpr char kServiceAccount[] = "service_account";

namespace google::scp::cpio::client_providers::test {
TEST(GcpKmsClientProviderUtilsTest, CreateAttestedCredentials) {
  string credentials;
  TeeGcpKmsClientProviderUtils::CreateAttestedCredentials(
      kWipProvider, kServiceAccount, credentials);
  EXPECT_EQ(credentials,
            "{\"audience\":\"//iam.googleapis.com/"
            "wip\",\"credential_source\":{\"file\":\"/run/container_launcher/"
            "attestation_verifier_claims_token\"},\"service_account_"
            "impersonation_url\":\"https://iamcredentials.googleapis.com/v1/"
            "projects/-/serviceAccounts/"
            "service_account:generateAccessToken\",\"subject_token_type\":"
            "\"urn:ietf:params:oauth:token-type:jwt\",\"token_url\":\"https://"
            "sts.googleapis.com/v1/token\",\"type\":\"external_account\"}");
}
}  // namespace google::scp::cpio::client_providers::test
