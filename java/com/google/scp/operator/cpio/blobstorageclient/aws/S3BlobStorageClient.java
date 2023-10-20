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

package com.google.scp.operator.cpio.blobstorageclient.aws;

import com.google.common.collect.ImmutableList;
import com.google.inject.Inject;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient;
import com.google.scp.operator.cpio.blobstorageclient.aws.S3BlobStorageClientModule.PartialRequestBufferSize;
import com.google.scp.operator.cpio.blobstorageclient.aws.S3BlobStorageClientModule.S3UsePartialRequests;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import java.io.InputStream;
import java.nio.file.Path;
import java.util.Optional;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CompletionException;
import software.amazon.awssdk.core.exception.SdkException;
import software.amazon.awssdk.services.s3.S3AsyncClient;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.DeleteObjectRequest;
import software.amazon.awssdk.services.s3.model.GetObjectRequest;
import software.amazon.awssdk.services.s3.model.ListObjectsV2Request;
import software.amazon.awssdk.services.s3.model.PutObjectRequest;
import software.amazon.awssdk.services.s3.model.S3Object;
import software.amazon.awssdk.services.s3.paginators.ListObjectsV2Iterable;
import software.amazon.awssdk.transfer.s3.S3TransferManager;
import software.amazon.awssdk.transfer.s3.model.UploadFileRequest;

/**
 * S3BlobStorageClient implements the {@code BlobStorageClient} interface for AWS S3 storage
 * service.
 */
public final class S3BlobStorageClient implements BlobStorageClient {
  private static String ACCOUNT_IDENTITY_NOT_SUPPORTED_MESSAGE =
      "Use of account identity is currently not supported for S3BlobStorageClient.";

  private final S3Client client;
  private final S3TransferManager transferManager;
  // Indicate whether to use http partial request in getBlob function.
  private final Boolean usePartialRequests;
  // The buffer size for http partial request.
  private final Integer partialRequestBufferSize;

  /** Creates a new instance of {@code S3BlobStorageClient}. */
  @Inject
  public S3BlobStorageClient(
      S3Client client,
      S3AsyncClient asyncClient,
      @S3UsePartialRequests Boolean usePartialRequests,
      @PartialRequestBufferSize Integer partialRequestBufferSize) {
    this.client = client;
    this.usePartialRequests = usePartialRequests;
    this.partialRequestBufferSize = partialRequestBufferSize;
    this.transferManager = S3TransferManager.builder().s3Client(asyncClient).build();
  }

  /**
   * If usePartialRequests is set to true, the http partial request is used. Otherwise, the original
   * http request would be sent. In the original http request, a single request is sent for each S3
   * object and returns a stream. This results in an error during an extended period of inactivity
   * as the connection is closed by the server. Http partial requests can solve this issue through
   * sending multiple requests for each object and retrieving partial data by specifying starting
   * byte and ending byte. The partial retrieved data would be stored in memory thus avoid
   * long-lived inactive connections.
   */
  @Override
  public InputStream getBlob(DataLocation location) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      if (usePartialRequests) {
        S3RangedStream s3RangedStream =
            new S3RangedStream(client, blobLocation, partialRequestBufferSize);
        s3RangedStream.checkFileExists();
        return s3RangedStream;
      } else {
        return client.getObject(
            GetObjectRequest.builder()
                .bucket(blobLocation.bucket())
                .key(blobLocation.key())
                .build());
      }
    } catch (SdkException exception) {
      throw new BlobStorageClientException(exception);
    }
  }

  @Override
  public InputStream getBlob(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    throw new UnsupportedOperationException(ACCOUNT_IDENTITY_NOT_SUPPORTED_MESSAGE);
  }

  @Override
  public void putBlob(DataLocation location, Path filePath) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      transferManager
          .uploadFile(
              UploadFileRequest.builder()
                  .putObjectRequest(
                      PutObjectRequest.builder()
                          .bucket(blobLocation.bucket())
                          .key(blobLocation.key())
                          .build())
                  .source(filePath)
                  .build())
          .completionFuture()
          .join();
    } catch (CompletionException exception) {
      // Underlying exception contains more details.
      throw new BlobStorageClientException(exception.getCause());
    } catch (CancellationException exception) {
      throw new BlobStorageClientException(exception);
    }
  }

  @Override
  public void putBlob(DataLocation location, Path filePath, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    throw new UnsupportedOperationException(ACCOUNT_IDENTITY_NOT_SUPPORTED_MESSAGE);
  }

  @Override
  public void deleteBlob(DataLocation location) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      client.deleteObject(
          DeleteObjectRequest.builder()
              .bucket(blobLocation.bucket())
              .key(blobLocation.key())
              .build());
    } catch (SdkException exception) {
      throw new BlobStorageClientException(exception);
    }
  }

  @Override
  public void deleteBlob(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    throw new UnsupportedOperationException(ACCOUNT_IDENTITY_NOT_SUPPORTED_MESSAGE);
  }

  @Override
  public ImmutableList<String> listBlobs(DataLocation location) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      ListObjectsV2Iterable objectsV2Iterable =
          client.listObjectsV2Paginator(
              ListObjectsV2Request.builder()
                  .bucket(blobLocation.bucket())
                  .prefix(blobLocation.key())
                  .build());
      return objectsV2Iterable.contents().stream()
          .map(S3Object::key)
          .collect(ImmutableList.toImmutableList());
    } catch (SdkException exception) {
      throw new BlobStorageClientException(exception);
    }
  }

  @Override
  public ImmutableList<String> listBlobs(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    throw new UnsupportedOperationException(ACCOUNT_IDENTITY_NOT_SUPPORTED_MESSAGE);
  }
}
