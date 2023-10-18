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

package com.google.scp.operator.cpio.blobstorageclient.gcp;

import com.google.cloud.storage.Storage;
import com.google.cloud.storage.StorageOptions;
import com.google.inject.Provides;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClientModule;
import com.google.scp.operator.cpio.blobstorageclient.gcp.Annotations.GcsEndpointUrl;
import com.google.scp.shared.clients.configclient.gcp.Annotations.GcpProjectId;
import java.util.Optional;

/** Guice Module for the Cloud Storage implementation of {@link BlobStorageClient} */
public final class GcsBlobStorageClientModule extends BlobStorageClientModule {

  /** Caller is expected to bind {@link GcpProjectId}. */
  public GcsBlobStorageClientModule() {}

  @Override
  public Class<? extends BlobStorageClient> getBlobStorageClientImplementation() {
    return GcsBlobStorageClient.class;
  }

  /** Provider for an instance of the {@code Storage} class */
  @Provides
  Storage provideCloudStorageClient(
      @GcpProjectId String gcpProjectId, @GcsEndpointUrl Optional<String> endpoint) {
    StorageOptions.Builder optionsBuilder = StorageOptions.newBuilder().setProjectId(gcpProjectId);
    endpoint.ifPresent(e -> optionsBuilder.setHost(e));
    return optionsBuilder.build().getService();
  }

  @Override
  public void configureModule() {}
}
