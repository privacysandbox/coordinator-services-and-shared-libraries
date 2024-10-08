/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.scp.operator.cpio.blobstorageclient.gcp;

import com.google.cloud.ReadChannel;
import com.google.cloud.storage.BlobId;
import com.google.cloud.storage.Storage;
import com.google.cloud.storage.StorageException;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.BlobStorageClientException;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

/** GCSRangedStream uses a partial request to retrieve object data from GCS. */
public class GCSRangedStream {

  public static InputStream getBlobRange(
      Storage storageClient, BlobStoreDataLocation dataLocation, int start, int length)
      throws BlobStorageClientException {
    BlobId blobId = BlobId.of(dataLocation.bucket(), dataLocation.key());
    ByteBuffer buffer = ByteBuffer.allocate(length);

    try (ReadChannel from = storageClient.reader(blobId)) {
      from.seek(start);
      from.read(buffer);

      buffer.position(0);
      return new ByteArrayInputStream(buffer.array());
    } catch (StorageException | IOException e) {
      throw new BlobStorageClientException(e);
    }
  }
}
