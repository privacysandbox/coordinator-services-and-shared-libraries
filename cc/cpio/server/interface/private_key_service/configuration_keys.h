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

namespace google::scp::cpio {
// TODO: merge endpoint, region and account into one struct after support
// passing struct through ConfigProvider.
// Required. The primary endpoint for the service hosting the private keys.
static constexpr char kPrimaryPrivateKeyVendingServiceEndpoint[] =
    "cmrt_sdk_primary_private_key_vending_service_endpoint";
// Required. The region of the primary endpoint.
static constexpr char kPrimaryPrivateKeyVendingServiceRegion[] =
    "cmrt_sdk_primary_private_key_vending_service_region";
// Required. The account identity to access the primary endpoint.
static constexpr char kPrimaryPrivateKeyVendingServiceAccountIdentity[] =
    "cmrt_sdk_primary_private_key_vending_service_account_identity";
// Required. List of secondary endpoints for the services hosting the private
// keys.
static constexpr char kSecondaryPrivateKeyVendingServiceEndpoints[] =
    "cmrt_sdk_secondary_private_key_vending_service_endpoints";
// Required. The regions of the secondary endpoint. The order should match the
// kSecondaryPrivateKeyVendingServiceEndpoints list.
static constexpr char kSecondaryPrivateKeyVendingServiceRegions[] =
    "cmrt_sdk_secondary_private_key_vending_service_regions";
// Required. The account identities to access the secondary endpoints. The
// order should match the kSecondaryPrivateKeyVendingServiceEndpoints list.
static constexpr char kSecondaryPrivateKeyVendingServiceAccountIdentities[] =
    "cmrt_sdk_secondary_private_key_vending_service_account_identities";
// Required for GCP. The cloudfunction url for the primary endpoint.
static constexpr char kGcpPrimaryPrivateKeyVendingServiceCloudfunctionUrl[] =
    "cmrt_sdk_gcp_primary_private_key_vending_service_cloudfunction_url";
// Required for GCP. The cloudfunction url for the secondary endpoint. The
// order should match the kSecondaryPrivateKeyVendingServiceEndpoints list.
static constexpr char kGcpSecondaryPrivateKeyVendingServiceCloudfunctionUrls[] =
    "cmrt_sdk_gcp_secondary_private_key_vending_service_cloudfunction_"
    "urls";
// Required for GCP. The WIP provider for the primary endpoint.
static constexpr char kGcpPrimaryPrivateKeyVendingServiceWipProvider[] =
    "cmrt_sdk_gcp_primary_private_key_vending_service_wip_provider";
// Required for GCP. The WIP provider for the secondary endpoint. The
// order should match the kSecondaryPrivateKeyVendingServiceEndpoints list.
static constexpr char kGcpSecondaryPrivateKeyVendingServiceWipProviders[] =
    "cmrt_sdk_gcp_secondary_private_key_vending_service_wip_providers";

// Optional. If not set, use the default value 2. The number of
// CompletionQueues for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kPrivateKeyClientCompletionQueueCount[] =
    "cmrt_sdk_private_key_client_completion_queue_count";
// Optional. If not set, use the default value 2. Minimum number of polling
// threads for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kPrivateKeyClientMinPollers[] =
    "cmrt_sdk_private_key_client_min_pollers";
// Optional. If not set, use the default value 5. Maximum number of polling
// threads for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kPrivateKeyClientMaxPollers[] =
    "cmrt_sdk_private_key_client_max_pollers";
// Optional. If not set, use the default value 2.
static constexpr char kPrivateKeyClientCpuThreadCount[] =
    "cmrt_sdk_private_key_client_cpu_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kPrivateKeyClientCpuThreadPoolQueueCap[] =
    "cmrt_sdk_private_key_client_cpu_thread_pool_queue_cap";
// Optional. If not set, use the default value 2.
static constexpr char kPrivateKeyClientIoThreadCount[] =
    "cmrt_sdk_private_key_client_io_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kPrivateKeyClientIoThreadPoolQueueCap[] =
    "cmrt_sdk_private_key_client_io_thread_pool_queue_cap";
}  // namespace google::scp::cpio
