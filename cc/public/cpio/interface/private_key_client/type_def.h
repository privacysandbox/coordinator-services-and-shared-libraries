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

#ifndef SCP_CPIO_INTERFACE_PRIVATE_KEY_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_PRIVATE_KEY_CLIENT_TYPE_DEF_H_

#include <string>
#include <vector>

#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio {
using PrivateKeyVendingServiceEndpoint = std::string;
using GcpPrivateKeyVendingServiceCloudfunctionUrl = std::string;
using GcpWIPProvider = std::string;

/// Configurations for private key vending endpoint.
struct PrivateKeyVendingEndpoint {
  virtual ~PrivateKeyVendingEndpoint() = default;

  /** @brief The account identity to access the cloud. This is used to create
   *  temporary credentials to access resources you normally has no access. In
   *  AWS, it is the IAM Role ARN. In GCP, it would be the service account.
   */
  AccountIdentity account_identity;
  /// The region of private key vending service.
  Region service_region;
  /// The endpoint of private key vending service.
  PrivateKeyVendingServiceEndpoint private_key_vending_service_endpoint;

  /// Only needed for GCP. Cloudfunction url for the private key vending
  /// service.
  GcpPrivateKeyVendingServiceCloudfunctionUrl
      gcp_private_key_vending_service_cloudfunction_url;
  /// Only needed for GCP. Pool to provide workload identity.
  /// Refer to
  /// https://cloud.google.com/iam/docs/workload-identity-federation#pools for
  /// details.
  GcpWIPProvider gcp_wip_provider;
};

/// Configuration for PrivateKeyClient.
struct PrivateKeyClientOptions {
  virtual ~PrivateKeyClientOptions() = default;

  /** @brief This endpoint hosts part of the private key. It is the source of
   * truth. If the part is missing here, the private key is treated as invalid
   * even the other parts can be found in other endpoints.
   */
  PrivateKeyVendingEndpoint primary_private_key_vending_endpoint;
  /// This list of endpoints host the remaining parts of the private key.
  std::vector<PrivateKeyVendingEndpoint>
      secondary_private_key_vending_endpoints;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_PRIVATE_KEY_CLIENT_TYPE_DEF_H_
