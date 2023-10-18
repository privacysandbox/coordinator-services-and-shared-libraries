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

#ifndef SCP_CPIO_INTERFACE_BLOB_STORAGE_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_BLOB_STORAGE_CLIENT_TYPE_DEF_H_

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace google::scp::cpio {
/// Configurations for BlobStorageClient.
struct BlobStorageClientOptions {
  // GCP - How long a blob storage transfer (download or upload) should stay
  // alive for after some duration of inaction.
  std::chrono::seconds transfer_stall_timeout = std::chrono::seconds(60 * 2);
  // GCP - How many retries should be used for blob storage operations.
  size_t retry_limit = 3;

  virtual ~BlobStorageClientOptions() = default;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_BLOB_STORAGE_CLIENT_TYPE_DEF_H_
