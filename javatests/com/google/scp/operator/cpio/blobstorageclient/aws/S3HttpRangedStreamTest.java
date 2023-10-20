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

import static com.google.common.truth.Truth.assertThat;
import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.acai.Acai;
import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.google.common.collect.ImmutableList;
import com.google.common.hash.HashCode;
import com.google.common.hash.HashFunction;
import com.google.common.hash.Hashing;
import com.google.inject.AbstractModule;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.BlobStorageClientException;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import com.google.scp.shared.testutils.aws.AwsHermeticTestHelper;
import com.google.scp.shared.testutils.aws.LocalStackAwsClientUtil;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.testcontainers.containers.localstack.LocalStackContainer;
import org.testcontainers.containers.wait.strategy.Wait;
import software.amazon.awssdk.services.s3.S3AsyncClient;
import software.amazon.awssdk.services.s3.S3Client;

/**
 * Test for the S3HttpRangedStream. Tests in this class includes different read functions/method in
 * S3HttpRangedStream, different read size and different buffer size. And compare hashed message
 * from reading with original hashed result from S3.
 */
@RunWith(JUnit4.class)
public class S3HttpRangedStreamTest {

  @Rule public Acai acai = new Acai(TestEnv.class);
  @Rule public TemporaryFolder folder = new TemporaryFolder();
  private static final Supplier<LocalStackContainer> memoizedContainerSupplier =
      Suppliers.memoize(
          () -> AwsHermeticTestHelper.createContainer(LocalStackContainer.Service.S3));
  // Class rule for hermetic testing.
  @ClassRule public static LocalStackContainer localstack = memoizedContainerSupplier.get();

  private static final String testMessage = "abc".repeat(100);
  private static final int NUM_BYTES = testMessage.length();
  private static final String BUCKET_NAME = AwsHermeticTestHelper.getBucketName();
  private File file;
  private final S3Client s3Client = AwsHermeticTestHelper.createS3Client(localstack);
  private final S3AsyncClient s3AsyncClient =
      LocalStackAwsClientUtil.createS3AsyncClient(localstack);
  // The initialized buffer size in s3BlobStorageClient is ignored because we are not using getBlob
  // function in s3BlobStorageClient.
  private final S3BlobStorageClient s3BlobStorageClient =
      new S3BlobStorageClient(s3Client, s3AsyncClient, true, 20);
  private HashFunction hashFunction = Hashing.sha512();
  private final HashCode expectedResult = hashFunction.hashString(testMessage, UTF_8);
  private final BlobStoreDataLocation location =
      BlobStoreDataLocation.create(BUCKET_NAME, "keyname");

  @Before
  public void setUp() throws IOException, BlobStorageClientException {
    // Create text file to upload.
    try {
      file = folder.newFile("file.txt");
      FileWriter writer = new FileWriter(file);
      writer.write(testMessage);
      writer.close();
    } catch (IOException e) {
      e.printStackTrace();
      throw new IOException("Could not create test file in temp dir.");
    }
    // Upload the file to S3.
    s3BlobStorageClient.putBlob(DataLocation.ofBlobStoreDataLocation(location), file.toPath());
    localstack.waitingFor(Wait.forHealthcheck());
  }

  @After
  public void cleanUp() throws BlobStorageClientException {
    ImmutableList<String> keys =
        s3BlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(
                BlobStoreDataLocation.create(BUCKET_NAME, /* withPrefix= */ "")));
    for (String key : keys) {
      s3BlobStorageClient.deleteBlob(
          DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(BUCKET_NAME, key)));
    }
  }

  // Test read() function in S3RangedStream
  @Test
  public void readStreamWithOneByte() throws Exception {
    ByteArrayOutputStream readBytesStream = new ByteArrayOutputStream(NUM_BYTES);
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, 20);

    for (int i = 0; i < NUM_BYTES; i++) {
      readBytesStream.write(rangedStream.read());
    }
    // Read again when it's already end of file
    int readEof = rangedStream.read();
    readBytesStream.flush();
    readBytesStream.close();
    HashCode hashReadByteBuffer =
        hashFunction.hashBytes(ByteBuffer.wrap(readBytesStream.toByteArray()));

    assertThat(expectedResult).isEqualTo(hashReadByteBuffer);
    // read() function should return -1 when end of file
    assertThat(readEof).isEqualTo(-1);
  }

  // Test read(byte[] b) function in S3RangedStream.
  @Test
  public void readStreamIntoByteArray() throws Exception {
    byte[] readBytesArray = new byte[NUM_BYTES];
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, 20);

    int totalReadSize = rangedStream.read(readBytesArray);
    // Read again when it's already end of file.
    int readEof = rangedStream.read(new byte[1]);
    HashCode hashReadByteBuffer = hashFunction.hashBytes(ByteBuffer.wrap(readBytesArray));

    assertThat(expectedResult).isEqualTo(hashReadByteBuffer);
    assertThat(totalReadSize).isEqualTo(NUM_BYTES);
    // read(byte[] b) function should return -1 when end of file.
    assertThat(readEof).isEqualTo(-1);
  }

  // Test read(byte[] b) function in S3RangedStream with input byte array size larger than file
  // size.
  @Test
  public void readStreamIntoByteArray_byteArrayLargerThanFileSize() throws Exception {
    byte[] largeBytesArray = new byte[NUM_BYTES + 100];
    byte[] readBytesArray = new byte[NUM_BYTES];
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, 20);

    int totalReadSize = rangedStream.read(largeBytesArray);
    // Read again when it's already end of file
    int readEof = rangedStream.read(new byte[1]);
    System.arraycopy(largeBytesArray, 0, readBytesArray, 0, NUM_BYTES);
    HashCode hashReadByteBuffer = hashFunction.hashBytes(ByteBuffer.wrap(readBytesArray));

    assertThat(expectedResult).isEqualTo(hashReadByteBuffer);
    assertThat(totalReadSize).isEqualTo(NUM_BYTES);
    // read(byte[] b) function should return -1 when end of file
    assertThat(readEof).isEqualTo(-1);
  }

  // Test read(byte[] b) function in S3RangedStream with input byte array size smaller than file
  // size.
  @Test
  public void readStreamIntoByteArray_byteArraySmallerThanFileSize() throws Exception {
    int bufferSize = NUM_BYTES - 100;
    byte[] smallBytesArray = new byte[bufferSize];
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, 20);
    int totalReadSize = 0;
    int currentReadSize = 0;

    while (currentReadSize != -1) {
      totalReadSize += currentReadSize;
      currentReadSize = rangedStream.read(smallBytesArray);
    }

    assertThat(totalReadSize).isEqualTo(NUM_BYTES);
  }

  // Test read(byte[] b, int offset, int readSize) function in S3RangedStream with read size smaller
  // than buffer size.
  @Test
  public void readStreamIntoByteArrayWithOffset_readSizeSmallerThanBufferSize() throws Exception {
    int readSize = 3;
    List<Integer> expectedReadSizeList = new ArrayList<>(Collections.nCopies(100, readSize));
    expectedReadSizeList.add(-1);
    byte[] readBytesArray = new byte[NUM_BYTES];
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, 20);
    int currentReadSize = 0;
    int offset = 0;
    List<Integer> outputReadSize = new ArrayList<>();

    // When reach end of file, read() returns -1.
    while (currentReadSize != -1) {
      offset += currentReadSize;
      currentReadSize = rangedStream.read(readBytesArray, offset, readSize);
      outputReadSize.add(currentReadSize);
    }
    HashCode hashReadByteBuffer = hashFunction.hashBytes(ByteBuffer.wrap(readBytesArray));

    assertThat(expectedResult).isEqualTo(hashReadByteBuffer);
    assertThat(outputReadSize).isEqualTo(expectedReadSizeList);
  }

  // Test read(byte[] b, int offset, int readSize) function in S3RangedStream with read size is
  // multiplier of buffer size.
  @Test
  public void readStreamIntoByteArrayWithOffset_readSizeIsBufferSizeMultiplier() throws Exception {
    ImmutableList<Integer> readSizeList = ImmutableList.of(100, 100, 100, 100);
    ImmutableList<Integer> expectedReadSizeList = ImmutableList.of(100, 100, 100, -1);
    byte[] readBytesArray = new byte[NUM_BYTES];
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, 20);
    int currentReadSize = 0;
    int offset = 0;
    List<Integer> outputReadSize = new ArrayList<>();

    for (int i = 0; i < readSizeList.size(); i++) {
      offset += currentReadSize;
      currentReadSize = rangedStream.read(readBytesArray, offset, readSizeList.get(i));
      outputReadSize.add(currentReadSize);
    }
    HashCode hashReadByteBuffer = hashFunction.hashBytes(ByteBuffer.wrap(readBytesArray));

    assertThat(expectedResult).isEqualTo(hashReadByteBuffer);
    assertThat(outputReadSize).isEqualTo(expectedReadSizeList);
  }

  // Test read(byte[] b, int offset, int readSize) function in S3RangedStream with different read
  // size.
  @Test
  public void readStreamIntoByteArrayWithOffset_mixedReadSize() throws Exception {
    ImmutableList<Integer> readSizeList = ImmutableList.of(10, 190, 50, 60, 10); // Sum is 320.
    ImmutableList<Integer> expectedReadSizeList =
        ImmutableList.of(10, 190, 50, 50, -1); // Sum is 300 (excluding -1).
    byte[] readBytesArray = new byte[NUM_BYTES];
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, 20);
    int currentReadSize = 0;
    int offset = 0;
    List<Integer> outputReadSize = new ArrayList<>();

    for (int i = 0; i < readSizeList.size(); i++) {
      offset += currentReadSize;
      currentReadSize = rangedStream.read(readBytesArray, offset, readSizeList.get(i));
      outputReadSize.add(currentReadSize);
    }
    HashCode hashReadByteBuffer = hashFunction.hashBytes(ByteBuffer.wrap(readBytesArray));

    assertThat(expectedResult).isEqualTo(hashReadByteBuffer);
    assertThat(outputReadSize).isEqualTo(expectedReadSizeList);
  }

  // Test read(byte[] b, int offset, int readSize) function in S3RangedStream when S3RangedStream
  // buffer size equals file size.
  @Test
  public void readStreamIntoByteArrayWithOffset_bufferSizeEqualsFileSize() throws Exception {
    ImmutableList<Integer> readSizeList = ImmutableList.of(10, 190, 50, 60, 10); // Sum is 320.
    ImmutableList<Integer> expectedReadSizeList =
        ImmutableList.of(10, 190, 50, 50, -1); // Sum is 300 (excluding -1).
    byte[] readBytesArray = new byte[NUM_BYTES];
    // Initialize S3RangedStream buffer size the same as text message size.
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, NUM_BYTES);
    int currentReadSize = 0;
    int offset = 0;
    List<Integer> outputReadSize = new ArrayList<>();

    for (int i = 0; i < readSizeList.size(); i++) {
      offset += currentReadSize;
      currentReadSize = rangedStream.read(readBytesArray, offset, readSizeList.get(i));
      outputReadSize.add(currentReadSize);
    }
    HashCode hashReadByteBuffer = hashFunction.hashBytes(ByteBuffer.wrap(readBytesArray));

    assertThat(expectedResult).isEqualTo(hashReadByteBuffer);
    assertThat(outputReadSize).isEqualTo(expectedReadSizeList);
  }

  // Test read(byte[] b, int offset, int readSize) function in S3RangedStream when S3RangedStream
  // buffer size is larger than file size.
  @Test
  public void readStreamIntoByteArrayWithOffset_bufferSizeLargerThanFileSize() throws Exception {
    ImmutableList<Integer> readSizeList = ImmutableList.of(10, 190, 50, 60, 10); // Sum is 320.
    ImmutableList<Integer> expectedReadSizeList =
        ImmutableList.of(10, 190, 50, 50, -1); // Sum is 300 (excluding -1).
    byte[] readBytesArray = new byte[NUM_BYTES];
    // Initialize S3RangedStream buffer larger than text message size.
    S3RangedStream rangedStream = new S3RangedStream(s3Client, location, NUM_BYTES + 100);
    int currentReadSize = 0;
    int offset = 0;
    List<Integer> outputReadSize = new ArrayList<>();

    for (int i = 0; i < readSizeList.size(); i++) {
      offset += currentReadSize;
      currentReadSize = rangedStream.read(readBytesArray, offset, readSizeList.get(i));
      outputReadSize.add(currentReadSize);
    }
    HashCode hashReadByteBuffer = hashFunction.hashBytes(ByteBuffer.wrap(readBytesArray));

    assertThat(expectedResult).isEqualTo(hashReadByteBuffer);
    assertThat(outputReadSize).isEqualTo(expectedReadSizeList);
  }

  public static final class TestEnv extends AbstractModule {
    @Override
    protected final void configure() {
      bind(LocalStackContainer.class).toInstance(memoizedContainerSupplier.get());
    }
  }
}
