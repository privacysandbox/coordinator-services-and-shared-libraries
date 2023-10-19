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

package com.google.scp.operator.worker.reader.avro;

import com.google.inject.Inject;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.BlobStorageClientException;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation;
import com.google.scp.operator.worker.reader.RecordReader.RecordReadException;
import com.google.scp.operator.worker.reader.RecordReaderFactory;
import com.google.scp.protocol.avro.AvroReportsReaderFactory;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Optional;

/** Produces {@link LocalNioPathAvroRecordReader}s from {@link DataLocation} */
public class LocalNioPathAvroReaderFactory implements RecordReaderFactory {

  private final AvroReportsReaderFactory reportsReaderFactory;
  private final BlobStorageClient blobStorageClient;

  @Inject
  public LocalNioPathAvroReaderFactory(
      AvroReportsReaderFactory reportsReaderFactory, BlobStorageClient blobStorageClient) {
    this.reportsReaderFactory = reportsReaderFactory;
    this.blobStorageClient = blobStorageClient;
  }

  /**
   * Factory method to return {@link LocalNioPathAvroRecordReader} from Job.
   *
   * <p>Note that the {@link LocalNioPathAvroRecordReader} is responsible for closing the stream.
   */
  @Override
  public LocalNioPathAvroRecordReader of(DataLocation dataLocation) throws RecordReadException {
    try {
      switch (dataLocation.getKind()) {
        case LOCAL_NIO_PATH:
          return makeLocalNioReader(dataLocation.localNioPath());
        case BLOB_STORE_DATA_LOCATION:
          return makeBlobStorageClientReader(dataLocation, Optional.empty());
        default:
          throw new RecordReadException(
              new IllegalArgumentException(
                  "Unsupported reports location type: " + dataLocation.getKind()));
      }
    } catch (BlobStorageClientException e) {
      throw new RecordReadException(e);
    } catch (IOException e) {
      throw new RecordReadException(e);
    }
  }

  @Override
  public LocalNioPathAvroRecordReader of(
      DataLocation dataLocation, Optional<String> accountIdentity) throws RecordReadException {
    try {
      switch (dataLocation.getKind()) {
        case LOCAL_NIO_PATH:
          return makeLocalNioReader(dataLocation.localNioPath());
        case BLOB_STORE_DATA_LOCATION:
          return makeBlobStorageClientReader(dataLocation, accountIdentity);
        default:
          throw new RecordReadException(
              new IllegalArgumentException(
                  "Unsupported reports location type: " + dataLocation.getKind()));
      }
    } catch (BlobStorageClientException e) {
      throw new RecordReadException(e);
    } catch (IOException e) {
      throw new RecordReadException(e);
    }
  }

  private LocalNioPathAvroRecordReader makeLocalNioReader(Path nioPath) throws IOException {
    return new LocalNioPathAvroRecordReader(
        reportsReaderFactory.create(Files.newInputStream(nioPath)));
  }

  private LocalNioPathAvroRecordReader makeBlobStorageClientReader(
      DataLocation dataLocation, Optional<String> accountIdentity)
      throws BlobStorageClientException, IOException {

    InputStream inputData =
        accountIdentity.isPresent()
            ? blobStorageClient.getBlob(dataLocation, accountIdentity)
            : blobStorageClient.getBlob(dataLocation);
    return new LocalNioPathAvroRecordReader(reportsReaderFactory.create(inputData));
  }
}
