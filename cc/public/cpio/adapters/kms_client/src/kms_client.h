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

#include <memory>
#include <string>
#include <vector>

#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/kms_client/kms_client_interface.h"
#include "public/cpio/interface/kms_client/type_def.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/kms_service/v1/kms_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc KmsClientInterface
 */
class KmsClient : public KmsClientInterface {
 public:
  explicit KmsClient(const std::shared_ptr<KmsClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult Decrypt(
      core::AsyncContext<google::cmrt::sdk::kms_service::v1::DecryptRequest,
                         google::cmrt::sdk::kms_service::v1::DecryptResponse>
          decrypt_context) noexcept override;

 protected:
  std::shared_ptr<client_providers::KmsClientProviderInterface>
      kms_client_provider_;

 private:
  std::shared_ptr<KmsClientOptions> options_;
};
}  // namespace google::scp::cpio
