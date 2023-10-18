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

import com.google.api.gax.paging.Page;
import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.ImpersonatedCredentials;
import com.google.cloud.storage.Blob;
import com.google.cloud.storage.BlobId;
import com.google.cloud.storage.BlobInfo;
import com.google.cloud.storage.Storage;
import com.google.cloud.storage.Storage.BlobListOption;
import com.google.cloud.storage.StorageOptions;
import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import java.io.IOException;
import java.io.InputStream;
import java.nio.channels.Channels;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Optional;
import java.util.stream.StreamSupport;

/**
 * GcsBlobStorageClient implements the {@code BlobStorageClient} interface for Gcp (Gcs) cloud
 * storage service.
 */
// TODO: Add test coverage.
public final class GcsBlobStorageClient implements BlobStorageClient {

  private Storage client;

  /** Creates an instance of the {@code GcsBlobStorageClient} class. */
  @Inject
  public GcsBlobStorageClient(Storage client) {
    this.client = client;
  }

  @Override
  public InputStream getBlob(DataLocation location) throws BlobStorageClientException {
    return getBlob(location, Optional.empty());
  }

  @Override
  public InputStream getBlob(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    Storage storageClient = createGcsClient(accountIdentity, Scope.READ_ONLY);
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    return Channels.newInputStream(
        storageClient.get(BlobId.of(blobLocation.bucket(), blobLocation.key())).reader());
  }

  @Override
  public void putBlob(DataLocation location, Path filePath) throws BlobStorageClientException {
    putBlob(location, filePath, Optional.empty());
  }

  @Override
  public void putBlob(DataLocation location, Path filePath, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    Storage storageClient = createGcsClient(accountIdentity, Scope.READ_AND_WRITE);
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      storageClient.create(
          BlobInfo.newBuilder(BlobId.of(blobLocation.bucket(), blobLocation.key())).build(),
          Files.readAllBytes(filePath));
    } catch (IOException exception) {
      throw new BlobStorageClientException(exception);
    }
  }

  @Override
  public void deleteBlob(DataLocation location) throws BlobStorageClientException {
    deleteBlob(location, Optional.empty());
  }

  @Override
  public void deleteBlob(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    Storage storageClient = createGcsClient(accountIdentity, Scope.READ_AND_WRITE);
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    storageClient.delete(blobLocation.bucket(), blobLocation.key());
  }

  @Override
  public ImmutableList<String> listBlobs(DataLocation location) throws BlobStorageClientException {
    return listBlobs(location, Optional.empty());
  }

  @Override
  public ImmutableList<String> listBlobs(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    Storage storageClient = createGcsClient(accountIdentity, Scope.READ_ONLY);
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    Page<Blob> blobs =
        storageClient.list(blobLocation.bucket(), BlobListOption.prefix(blobLocation.key()));
    return StreamSupport.stream(blobs.iterateAll().spliterator(), /* parallel= */ false)
        .map(Blob::getBlobId)
        .map(BlobId::getName)
        .collect(ImmutableList.toImmutableList());
  }

  private Storage createGcsClient(Optional<String> accountIdentity, Scope scope)
      throws BlobStorageClientException {
    if (accountIdentity.isPresent()) {
      try {
        ImpersonatedCredentials credentials =
            ImpersonatedCredentials.newBuilder()
                .setSourceCredentials(GoogleCredentials.getApplicationDefault())
                .setTargetPrincipal(accountIdentity.get())
                .setScopes(Arrays.asList(scope.type))
                .build();
        return StorageOptions.newBuilder().setCredentials(credentials).build().getService();
      } catch (IOException exception) {
        throw new BlobStorageClientException(exception);
      }
    }
    return client;
  }

  /** Enum representation of the access scopes for GCS. */
  private enum Scope {
    /** Allows read operations on data in GCS. */
    READ_ONLY("https://www.googleapis.com/auth/devstorage.read_only"),

    /** Allows read and write operator on data in GCS. */
    READ_AND_WRITE("https://www.googleapis.com/auth/devstorage.read_write");

    public final String type;

    Scope(String type) {
      this.type = type;
    }
  }
}
