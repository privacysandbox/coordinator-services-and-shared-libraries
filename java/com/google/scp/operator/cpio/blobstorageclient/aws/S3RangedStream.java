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

import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.BlobStorageClientException;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import software.amazon.awssdk.core.ResponseInputStream;
import software.amazon.awssdk.core.exception.SdkException;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.GetObjectRequest;
import software.amazon.awssdk.services.s3.model.GetObjectResponse;

/**
 * S3RangedStream extends from InputStream. When calling read function, the data in the buffer would
 * be returned. If the buffer is empty, the http partial request would be sent to retrieve data from
 * the S3 object.
 */
public class S3RangedStream extends InputStream {

  private final S3Client client;
  private final BlobStoreDataLocation dataLocation;
  // A cursor to record the start point of http partial request.
  private long currentFileOffset = 0;
  // The buffer size for http request.
  private final int rangedHttpBufferSize;
  // The current available data size in the buffer.
  private int availableBufferSize = 0;
  // Buffer for storing data from http partial request.
  private ByteBuffer buffer;
  // A flag to indicate whether http request reaches the end of the file.
  private boolean isEndOfFile = false;

  public S3RangedStream(
      S3Client client, BlobStoreDataLocation dataLocation, Integer rangedHttpBufferSize) {
    this.client = client;
    this.dataLocation = dataLocation;
    this.rangedHttpBufferSize = rangedHttpBufferSize;
  }

  /**
   * This function sends GetObjectRequest to retrieve partial data from an S3 object. The partial
   * data range can be specified in start and end parameters.
   */
  private ResponseInputStream<GetObjectResponse> getBlobRange(long start, long end)
      throws BlobStorageClientException {
    try {
      return client.getObject(
          GetObjectRequest.builder()
              .bucket(dataLocation.bucket())
              .key(dataLocation.key())
              .range("bytes=" + start + "-" + end)
              .build());
    } catch (SdkException exception) {
      throw new BlobStorageClientException(exception);
    }
  }

  /**
   * Send a getObject request to check if the file exists. If not, BlobStorageClientException would
   * be thrown.
   */
  public void checkFileExists() throws BlobStorageClientException {
    // Use try-with-resources to close the stream when exiting.
    try (ResponseInputStream<GetObjectResponse> rangeResponse = getBlobRange(0, 1)) {
    } catch (IOException e) {
      throw new BlobStorageClientException(e);
    }
  }

  // Sends a partial request to fetch the next chunk of data from the server.
  private void requestHttpData() throws IOException, BlobStorageClientException {
    // The start and end bytes in http range request are inclusive.
    long httpRangeEnd = currentFileOffset + rangedHttpBufferSize - 1;
    try (ResponseInputStream<GetObjectResponse> rangeResponse =
        getBlobRange(currentFileOffset, httpRangeEnd)) {
      // Read the response into a byte buffer.
      buffer = ByteBuffer.wrap(rangeResponse.readAllBytes());
      // The contentRange response syntax is <unit> <range-start>-<range-end>/<size>. For example,
      // in "bytes 200-1000/67589", 67589 is total file length. The total file length can be parsed
      // through finding the index of "/" and using substring function.
      String responseString = rangeResponse.response().contentRange();
      String fileLengthString = responseString.substring(responseString.lastIndexOf('/') + 1);
      // If fileLengthString == "*', the total file length is unknown.
      if (fileLengthString.equals("*")) {
        throw new IOException(
            "The total file length is unknown. Currently, "
                + "S3RangedStream only supports when total file length is known.");
      }
      long fileLength = Long.parseLong(fileLengthString);
      // The responseSize would be the same as rangedHttpBufferSize except for last request which
      // may be less than rangedHttpBufferSize.
      long responseSize = rangeResponse.response().contentLength();
      currentFileOffset += responseSize;
      // The range of availableBufferSize is always between 0 and rangedHttpBufferSize.
      availableBufferSize += responseSize;
      if (currentFileOffset >= fileLength) {
        isEndOfFile = true;
      }
    }
  }

  private boolean hasNext() {
    return !isEndOfFile || availableBufferSize > 0;
  }

  public int read() throws IOException {
    byte[] tmpBuffer = new byte[1];
    int readSize = read(tmpBuffer, 0, 1);
    return (readSize == -1) ? readSize : Byte.toUnsignedInt(tmpBuffer[0]);
  }

  public int read(byte[] b) throws IOException {
    return read(b, 0, b.length);
  }

  public int read(byte[] b, int offset, int readSize) throws IOException {
    if (!hasNext()) {
      return -1;
    }
    int currentReadSize = 0;
    while (readSize > 0 && hasNext()) {
      // Check if there is available data in the buffer. If there is no data in the buffer, send a
      // http partial request to retrieve more data.
      if (availableBufferSize == 0) {
        try {
          requestHttpData();
        } catch (BlobStorageClientException e) {
          throw new IOException(e);
        }
      }
      int currentCopySize = Math.min(readSize, availableBufferSize);
      buffer.get(b, offset, currentCopySize);
      availableBufferSize -= currentCopySize;
      readSize -= currentCopySize;
      offset += currentCopySize;
      currentReadSize += currentCopySize;
    }
    return currentReadSize;
  }

  public int available() throws IOException {
    return availableBufferSize;
  }
}
