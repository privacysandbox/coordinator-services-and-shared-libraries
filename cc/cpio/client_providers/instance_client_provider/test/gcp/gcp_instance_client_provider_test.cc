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

#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"
#include "core/curl_client/mock/mock_curl_client.h"
#include "core/test/utils/conditional_wait.h"
#include "cpio/client_providers/auth_token_provider/mock/mock_auth_token_provider.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/error_codes.h"
#include "public/core/test/interface/execution_result_matchers.h"

using absl::StrCat;
using absl::StrFormat;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::scp::core::AsyncContext;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED;
using google::scp::core::errors::SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::MockCurlClient;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::client_providers::GcpInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockAuthTokenProvider;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::chrono::seconds;
using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {
constexpr char kURIForProjectId[] =
    "http://metadata.google.internal/computeMetadata/v1/project/project-id";
constexpr char kURIForInstanceId[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/id";
constexpr char kURIForInstanceZone[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/zone";
constexpr char kMetadataFlavorHeaderKey[] = "Metadata-Flavor";
constexpr char kMetadataFlavorHeaderValue[] = "Google";
constexpr char kProjectIdResult[] = "123456";
constexpr char kZoneResult[] = "projects/123456/zones/us-central1-c";
constexpr char kInstanceIdResult[] = "1234567";
constexpr char kInstanceResourceName[] =
    R"(//compute.googleapis.com/projects/123456/zones/us-central1-c/instances/1234567)";
constexpr char kResourceId[] =
    "projects/123456/zones/us-central1-c/instances/1234567";
constexpr char kZoneMock[] = "us-central1-c";

constexpr char kGcpInstanceGetUrlPrefix[] =
    "https://compute.googleapis.com/compute/v1/";
constexpr char kSessionTokenMock[] = "session-token-test";
constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kBearerTokenPrefix[] = "Bearer ";

constexpr char kResourceManagerUriFormat[] =
    "https://%scloudresourcemanager.googleapis.com/v3/tagBindings";
constexpr char kParentParameter[] = "parent=";
constexpr char kPageSizeSetting[] = "pageSize=300";

}  // namespace

namespace google::scp::cpio::client_providers::test {
class GcpInstanceClientProviderTest : public testing::Test {
 protected:
  GcpInstanceClientProviderTest()
      : http1_client_(make_shared<MockCurlClient>()),
        http2_client_(make_shared<MockCurlClient>()),
        authorizer_provider_(make_shared<MockAuthTokenProvider>()),
        instance_provider_(make_unique<GcpInstanceClientProvider>(
            authorizer_provider_, http1_client_, http2_client_)) {
    EXPECT_SUCCESS(instance_provider_->Init());
    EXPECT_SUCCESS(instance_provider_->Run());

    get_details_path_mock_ = StrCat(kGcpInstanceGetUrlPrefix, kResourceId);
    get_details_request_ =
        make_shared<GetInstanceDetailsByResourceNameRequest>();
    get_details_request_->set_instance_resource_name(kInstanceResourceName);

    get_tag_path_mock_ =
        StrFormat(kResourceManagerUriFormat, absl::StrCat(kZoneMock, "-"));
    get_tags_request_ = make_shared<GetTagsByResourceNameRequest>();
    get_tags_request_->set_resource_name(kInstanceResourceName);
  }

  ~GcpInstanceClientProviderTest() {
    EXPECT_SUCCESS(instance_provider_->Stop());
  }

  shared_ptr<MockCurlClient> http1_client_;
  shared_ptr<MockCurlClient> http2_client_;
  shared_ptr<MockAuthTokenProvider> authorizer_provider_;
  unique_ptr<GcpInstanceClientProvider> instance_provider_;

  string get_details_path_mock_;
  shared_ptr<GetInstanceDetailsByResourceNameRequest> get_details_request_;
  string get_tag_path_mock_;
  shared_ptr<GetTagsByResourceNameRequest> get_tags_request_;
};

TEST_F(GcpInstanceClientProviderTest, GetCurrentInstanceResourceNameSync) {
  string project_id_result = kProjectIdResult;
  string zone_result = kZoneResult;
  string id_result = kInstanceIdResult;

  EXPECT_CALL(*http1_client_, PerformRequest)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;

        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));

        context.response = make_shared<HttpResponse>();
        if (*request.path == kURIForProjectId) {
          context.response->body = BytesBuffer(project_id_result);
        }
        if (*request.path == kURIForInstanceZone) {
          context.response->body = BytesBuffer(zone_result);
        }
        if (*request.path == kURIForInstanceId) {
          context.response->body = BytesBuffer(id_result);
        }
        context.result = SuccessExecutionResult();

        context.Finish();
        return SuccessExecutionResult();
      });

  string resource_name;

  EXPECT_THAT(
      instance_provider_->GetCurrentInstanceResourceNameSync(resource_name),
      IsSuccessful());

  EXPECT_EQ(resource_name, StrCat("//compute.googleapis.com/", kResourceId));
}

TEST_F(GcpInstanceClientProviderTest,
       GetCurrentInstanceResourceNameSyncFailedWithHttpPerformRequest) {
  EXPECT_CALL(*http1_client_, PerformRequest)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  string resource_name;
  EXPECT_THAT(
      instance_provider_->GetCurrentInstanceResourceNameSync(resource_name),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));

  EXPECT_EQ(resource_name, "");
}

TEST_F(GcpInstanceClientProviderTest, GetCurrentInstanceResourceName) {
  string project_id_result = kProjectIdResult;
  string zone_result = kZoneResult;
  string id_result = kInstanceIdResult;

  EXPECT_CALL(*http1_client_, PerformRequest)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;

        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));

        context.response = make_shared<HttpResponse>();
        if (*request.path == kURIForProjectId) {
          context.response->body = BytesBuffer(project_id_result);
        }
        if (*request.path == kURIForInstanceZone) {
          context.response->body = BytesBuffer(zone_result);
        }
        if (*request.path == kURIForInstanceId) {
          context.response->body = BytesBuffer(id_result);
        }
        context.result = SuccessExecutionResult();

        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<size_t> condition{0};
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_EQ(context.response->instance_resource_name(),
                      StrCat("//compute.googleapis.com/", kResourceId));
            condition++;
          });

  EXPECT_THAT(instance_provider_->GetCurrentInstanceResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load() == 1; });
}

TEST_F(GcpInstanceClientProviderTest,
       FailedToGetCurrentInstanceResourceNameOnlyGotOneResult) {
  string id_result = kInstanceIdResult;

  EXPECT_CALL(*http1_client_, PerformRequest)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;

        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));

        context.response = make_shared<HttpResponse>();
        if (*request.path == kURIForProjectId) {
          context.result = FailureExecutionResult(SC_UNKNOWN);
        }
        if (*request.path == kURIForInstanceZone) {
          context.result = FailureExecutionResult(SC_UNKNOWN);
        }
        if (*request.path == kURIForInstanceId) {
          context.response->body = BytesBuffer(id_result);
          context.result = SuccessExecutionResult();
        }
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<size_t> condition{0};
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            condition++;
          });

  EXPECT_THAT(instance_provider_->GetCurrentInstanceResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load() == 1; });
}

TEST_F(GcpInstanceClientProviderTest, FailedToGetCurrentInstanceResourceName) {
  EXPECT_CALL(*http1_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;

        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));
        return FailureExecutionResult(SC_UNKNOWN);
      });

  atomic<size_t> condition{0};
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            condition++;
          });

  EXPECT_THAT(instance_provider_->GetCurrentInstanceResourceName(context),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  WaitUntil([&]() { return condition.load() == 1; });
}

TEST_F(GcpInstanceClientProviderTest, GetInstanceDetailsSyncSuccess) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response, with some fields of no interest
  // removed.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
          "labels": {
            "test-label-1": "test-value-1",
            "test-label-2": "test-value-2"
          },
          "networkInterfaces": [
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.10.0.99",
              "name": "nic0",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "natIP": "255.255.255.01",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "Hb2S=",
              "stackType": "IPV4_ONLY"
            },
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.125.0.53",
              "name": "nic1",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "natIP": "255.255.255.02",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "IR29OyjoQ88=",
              "stackType": "IPV4_ONLY"
            }
          ]
        }
      )""";

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  InstanceDetails details;
  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceNameSync(
                  kInstanceResourceName, details),
              IsSuccessful());
  EXPECT_EQ(details.instance_id(), "123456789");
  EXPECT_EQ(details.networks().size(), 2);
  EXPECT_EQ(details.networks(0).public_ipv4_address(), "255.255.255.01");
  EXPECT_EQ(details.networks(0).private_ipv4_address(), "10.10.0.99");
  EXPECT_EQ(details.networks(1).public_ipv4_address(), "255.255.255.02");
  EXPECT_EQ(details.networks(1).private_ipv4_address(), "10.125.0.53");

  EXPECT_THAT(details.labels(),
              UnorderedElementsAre(Pair("test-label-1", "test-value-1"),
                                   Pair("test-label-2", "test-value-2")));
}

TEST_F(GcpInstanceClientProviderTest, GetInstanceDetailsAccessConfigLoop) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response, with some fields of no interest
  // removed.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
          "networkInterfaces": [
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.10.0.99",
              "name": "nic0",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "networkTier": "PREMIUM"
                },
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "natIP": "255.255.255.02",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "Hb2S=",
              "stackType": "IPV4_ONLY"
            }
          ]
        }
      )""";

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  InstanceDetails details;
  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceNameSync(
                  kInstanceResourceName, details),
              IsSuccessful());
  EXPECT_EQ(details.instance_id(), "123456789");
  EXPECT_EQ(details.networks().size(), 1);
  EXPECT_EQ(details.networks(0).private_ipv4_address(), "10.10.0.99");
  EXPECT_EQ(details.networks(0).public_ipv4_address(), "255.255.255.02");
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsSyncFailedWithHttpRequest) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  InstanceDetails details;
  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceNameSync(
                  kInstanceResourceName, details),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST_F(GcpInstanceClientProviderTest, GetInstanceDetailsSuccess) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response, with some fields of no interest
  // removed.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
          "networkInterfaces": [
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.10.0.99",
              "name": "nic0",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "natIP": "255.255.255.01",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "Hb2S=",
              "stackType": "IPV4_ONLY"
            }
          ]
        }
      )""";

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_SUCCESS(context.result);
            const auto& details = context.response->instance_details();
            EXPECT_EQ(details.instance_id(), "123456789");
            EXPECT_EQ(details.networks().size(), 1);
            EXPECT_EQ(details.networks(0).public_ipv4_address(),
                      "255.255.255.01");
            EXPECT_EQ(details.networks(0).private_ipv4_address(), "10.10.0.99");
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsSuccessWithoutPublicIP) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response without public IP, with some fields
  // of no interest removed.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
          "networkInterfaces": [
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.10.0.99",
              "name": "nic0",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "Hb2S=",
              "stackType": "IPV4_ONLY"
            }
          ]
        }
      )""";

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_SUCCESS(context.result);
            EXPECT_EQ(context.response->instance_details().instance_id(),
                      "123456789");
            EXPECT_THAT(context.response->instance_details()
                            .networks(0)
                            .public_ipv4_address(),
                        IsEmpty());
            EXPECT_EQ(context.response->instance_details()
                          .networks(0)
                          .private_ipv4_address(),
                      "10.10.0.99");
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsFailedWithInvalidInstanceResourceName) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken).Times(0);
  EXPECT_CALL(*http2_client_, PerformRequest).Times(0);

  auto get_details_request_bad =
      make_shared<GetInstanceDetailsByResourceNameRequest>();
  // Resource ID is not a valid instance resource name without a valid prefix.
  get_details_request_bad->set_instance_resource_name(kResourceId);

  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(get_details_request_bad),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              ResultIs(FailureExecutionResult(
                  SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)));
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsFailedWithCredentialSigningFailure) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*http2_client_, PerformRequest).Times(0);

  atomic<bool> condition{false};
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsFailedWithHttpPerformSignedRequest) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        auto request = context.request;
        EXPECT_EQ(request->method, HttpMethod::GET);
        EXPECT_THAT(request->path, Pointee(Eq(get_details_path_mock_)));
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsFailedMalformedHttpResponse) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  // Malformed http response without `networkInterfaces` field.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
        }
      )""";

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  auto failure = FailureExecutionResult(
      SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED);
  atomic<bool> condition{false};
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result, ResultIs(failure));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetInstanceDetailsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest, GetTagsByResourceNameSuccess) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response, with some fields of no interest
  // removed.
  auto tags_response_mock = R"""(
          {
            "tagBindings": [
                {
                  "name": "name_1",
                  "parent": "parent_1",
                  "tagValue": "value_1"
                },
                {
                  "name": "name_2",
                  "parent": "parent_2",
                  "tagValue": "value_2"
                },
                {
                  "name": "name_3",
                  "parent": "parent_3",
                  "tagValue": "value_3"
                }
            ],
            "nextPageToken": ""
          }
        )""";

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_tag_path_mock_)));
        EXPECT_THAT(request.query, Pointee(absl::StrCat(
                                       kParentParameter, kInstanceResourceName,
                                       "&", kPageSizeSetting)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(tags_response_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(move(get_tags_request_),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                EXPECT_SUCCESS(context.result);
                EXPECT_THAT(context.response->tags(),
                            UnorderedElementsAre(Pair("name_1", "value_1"),
                                                 Pair("name_2", "value_2"),
                                                 Pair("name_3", "value_3")));
                condition.store(true);
              });

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithCredentialSigningFailure) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*http2_client_, PerformRequest).Times(0);

  atomic<bool> condition{false};
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(move(get_tags_request_),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                EXPECT_THAT(context.result,
                            ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                condition.store(true);
              });

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithHttpPerformSignedRequest) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        auto request = context.request;
        EXPECT_EQ(request->method, HttpMethod::GET);
        EXPECT_THAT(request->path, Pointee(Eq(get_tag_path_mock_)));
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(move(get_tags_request_),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                EXPECT_THAT(context.result,
                            ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                condition.store(true);
              });

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest,
       GetTagsByResourceNameFailedMalformedHttpResponse) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  // Malformed http response in which one tagBinding missing 'tagValue'
  auto tags_response_mock = R"""(
          {
            "tagBindings": [
                {
                  "name": "name_1",
                  "parent": "parent_1",
                },
                {
                  "name": "name_2",
                  "parent": "parent_2",
                  "tagValue": "value_2"
                },
                {
                  "name": "name_3",
                  "parent": "parent_3",
                  "tagValue": "value_3"
                }
            ],
            "nextPageToken": ""
          }
        )""";

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_tag_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(tags_response_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(
          move(get_tags_request_),
          [&](AsyncContext<GetTagsByResourceNameRequest,
                           GetTagsByResourceNameResponse>& context) {
            EXPECT_THAT(
                context.result,
                ResultIs(FailureExecutionResult(
                    SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED)));
            condition.store(true);
          });

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

TEST_F(GcpInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithEmptyHttpResponse) {
  EXPECT_CALL(*authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            make_shared<string>(kSessionTokenMock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  // Empty http response
  auto tags_response_mock = R"({})";

  EXPECT_CALL(*http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_tag_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = make_shared<HttpResponse>();
        context.response->body = BytesBuffer(tags_response_mock);
        context.result = SuccessExecutionResult();
        context.Finish();
        return SuccessExecutionResult();
      });

  atomic<bool> condition{false};
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(move(get_tags_request_),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                EXPECT_SUCCESS(context.result);
                EXPECT_THAT(context.response->tags(), IsEmpty());
                condition.store(true);
              });

  EXPECT_THAT(instance_provider_->GetTagsByResourceName(context),
              IsSuccessful());
  WaitUntil([&]() { return condition.load(); });
}

}  // namespace google::scp::cpio::client_providers::test
