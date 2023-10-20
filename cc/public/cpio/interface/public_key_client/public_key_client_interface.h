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

#ifndef SCP_CPIO_INTERFACE_PUBLIC_KEY_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_PUBLIC_KEY_CLIENT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for fetching public key from Key Management
 * Service.
 *
 * Use PublicKeyClientFactory::Create to create the PublicKeyClient. Call
 * PublicKeyClientInterface::Init and PublicKeyClientInterface::Run before
 * actually use it, and call PublicKeyClientInterface::Stop when finish using
 * it.
 */
class PublicKeyClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Lists a list of public keys.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult ListPublicKeys(
      cmrt::sdk::public_key_service::v1::ListPublicKeysRequest request,
      Callback<cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>
          callback) noexcept = 0;
};

/// Factory to create PublicKeyClient.
class PublicKeyClientFactory {
 public:
  /**
   * @brief Creates PublicKeyClient.
   *
   * @param options configurations for PublicKeyClient.
   * @return std::unique_ptr<PublicKeyClientInterface> PublicKeyClient object.
   */
  static std::unique_ptr<PublicKeyClientInterface> Create(
      PublicKeyClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_PUBLIC_KEY_CLIENT_INTERFACE_H_
