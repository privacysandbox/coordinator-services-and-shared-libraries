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
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.google.common.collect.ImmutableList;
import com.google.common.hash.HashCode;
import com.google.common.hash.HashFunction;
import com.google.common.hash.Hashing;
import com.google.common.io.ByteStreams;
import com.google.inject.AbstractModule;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.BlobStorageClientException;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import com.google.scp.shared.testutils.aws.AwsHermeticTestHelper;
import com.google.scp.shared.testutils.aws.LocalStackAwsClientUtil;
import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
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

/** Hermetic Test for the S3BlobStorageClient. */
@RunWith(JUnit4.class)
public class S3BlobStorageClientTest {

  @Rule public Acai acai = new Acai(TestEnv.class);
  @Rule public TemporaryFolder folder = new TemporaryFolder();
  private static final Supplier<LocalStackContainer> memoizedContainerSupplier =
      Suppliers.memoize(
          () -> AwsHermeticTestHelper.createContainer(LocalStackContainer.Service.S3));
  // Class rule for hermetic testing.
  @ClassRule public static LocalStackContainer localstack = memoizedContainerSupplier.get();

  private static final String testMessage = "S3 Hermetic Test";
  private static final int NUM_BYTES = 16;
  private static final String BUCKET_NAME = AwsHermeticTestHelper.getBucketName();
  private File file;
  private S3Client s3Client = AwsHermeticTestHelper.createS3Client(localstack);
  private S3AsyncClient s3AsyncClient = LocalStackAwsClientUtil.createS3AsyncClient(localstack);
  private S3BlobStorageClient s3BlobStorageClient =
      new S3BlobStorageClient(s3Client, s3AsyncClient, false, 0);
  private S3BlobStorageClient s3BlobStorageClientWithRangedStream =
      new S3BlobStorageClient(s3Client, s3AsyncClient, true, 180000);

  @Before
  public void setUp() throws IOException {
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

  @Test
  public void getBlob_consistentObjectRetrieval() throws Exception {
    // Compare hashed message with hashed result from S3.
    HashFunction hashFunction = Hashing.sha512();
    HashCode hashPreUpload = hashFunction.hashString(testMessage, UTF_8);
    ByteArrayOutputStream downloadedBytesStream = new ByteArrayOutputStream(NUM_BYTES);

    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(BUCKET_NAME, "keyname"));
    s3BlobStorageClient.putBlob(location, file.toPath());
    InputStream download = s3BlobStorageClient.getBlob(location);
    BufferedInputStream input = new BufferedInputStream(download);
    InputStream sizedInputStream = ByteStreams.limit(input, NUM_BYTES);
    ByteStreams.copy(sizedInputStream, downloadedBytesStream);
    downloadedBytesStream.flush();
    downloadedBytesStream.close();
    ByteBuffer downloadedBytesBuffer = ByteBuffer.wrap(downloadedBytesStream.toByteArray());
    HashCode hashPostUpload = hashFunction.hashBytes(downloadedBytesBuffer);

    assertThat(hashPreUpload).isEqualTo(hashPostUpload);
  }

  @Test
  public void getBlob_exceptionOnMissingObject() throws Exception {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            s3BlobStorageClient.getBlob(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create(BUCKET_NAME, "NonExistentKey"))));
  }

  @Test
  public void deleteBlob_successfulDeleteThenThrowsOnGet() throws Exception {
    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(
            BlobStoreDataLocation.create(BUCKET_NAME, "toBeDeleted"));
    s3BlobStorageClient.putBlob(location, file.toPath());
    s3BlobStorageClient.deleteBlob(location);

    assertThrows(BlobStorageClientException.class, () -> s3BlobStorageClient.getBlob(location));
  }

  @Test
  public void putBlob_NoSuchBucketException() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            s3BlobStorageClient.putBlob(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create("no-such-bucket", /* withPrefix= */ "")),
                file.toPath()));
  }

  @Test
  public void listBlobs_consistentKeyRetrieval() throws Exception {
    String keyName = "keyName";
    s3BlobStorageClient.putBlob(
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(BUCKET_NAME, keyName)),
        file.toPath());

    ImmutableList<String> keys =
        s3BlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(
                BlobStoreDataLocation.create(BUCKET_NAME, /* withPrefix= */ "")));

    assertThat(keys).hasSize(1);
    assertThat(keys).containsExactly(keyName);
  }

  @Test
  public void listBlobs_listAllKeys() throws Exception {
    ImmutableList<String> keys = ImmutableList.of("key1", "key2", "1key");
    ImmutableList<String> expectedKeys = ImmutableList.of("key1", "key2", "1key");
    String prefix = "";
    for (String key : keys) {
      s3BlobStorageClient.putBlob(
          DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(BUCKET_NAME, key)),
          file.toPath());
    }

    ImmutableList<String> filteredKeys =
        s3BlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(
                BlobStoreDataLocation.create(BUCKET_NAME, prefix)));

    assertThat(filteredKeys).containsExactlyElementsIn(expectedKeys);
  }

  @Test
  public void listBlobs_listOneKeyOutOfMany() throws Exception {
    ImmutableList<String> keys = ImmutableList.of("key1", "key2", "1key");
    ImmutableList<String> expectedKeys = ImmutableList.of("key1");
    String prefix = "key1";
    for (String key : keys) {
      s3BlobStorageClient.putBlob(
          DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(BUCKET_NAME, key)),
          file.toPath());
    }

    ImmutableList<String> filteredKeys =
        s3BlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(
                BlobStoreDataLocation.create(BUCKET_NAME, prefix)));

    assertThat(filteredKeys).containsExactlyElementsIn(expectedKeys);
  }

  @Test
  public void listBlobs_listKeysWithPartialPrefixMatch() throws Exception {
    ImmutableList<String> keys = ImmutableList.of("key1", "key2", "1key");
    ImmutableList<String> expectedKeys = ImmutableList.of("key1", "key2");
    String prefix = "key";
    for (String key : keys) {
      s3BlobStorageClient.putBlob(
          DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(BUCKET_NAME, key)),
          file.toPath());
    }

    ImmutableList<String> filteredKeys =
        s3BlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(
                BlobStoreDataLocation.create(BUCKET_NAME, prefix)));

    assertThat(filteredKeys).containsExactlyElementsIn(expectedKeys);
  }

  @Test
  public void listBlobs_NoSuchBucketException() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            s3BlobStorageClient.listBlobs(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create("no-such-bucket", /* withPrefix= */ ""))));
  }

  @Test
  public void getBlob_consistentObjectRetrieval_rangedStream() throws Exception {
    // Compare hashed message with hashed result from S3.
    HashFunction hashFunction = Hashing.sha512();
    HashCode hashPreUpload = hashFunction.hashString(testMessage, UTF_8);
    ByteArrayOutputStream downloadedBytesStream = new ByteArrayOutputStream(NUM_BYTES);

    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(BUCKET_NAME, "keyname"));
    s3BlobStorageClientWithRangedStream.putBlob(location, file.toPath());
    InputStream download = s3BlobStorageClientWithRangedStream.getBlob(location);
    BufferedInputStream input = new BufferedInputStream(download);
    InputStream sizedInputStream = ByteStreams.limit(input, NUM_BYTES);
    ByteStreams.copy(sizedInputStream, downloadedBytesStream);
    downloadedBytesStream.flush();
    downloadedBytesStream.close();
    ByteBuffer downloadedBytesBuffer = ByteBuffer.wrap(downloadedBytesStream.toByteArray());
    HashCode hashPostUpload = hashFunction.hashBytes(downloadedBytesBuffer);

    assertThat(hashPreUpload).isEqualTo(hashPostUpload);
  }

  @Test
  public void getBlob_exceptionOnMissingObject_rangedStream() throws Exception {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            s3BlobStorageClientWithRangedStream.getBlob(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create(BUCKET_NAME, "NonExistentKey"))));
  }

  public static final class TestEnv extends AbstractModule {
    @Override
    protected final void configure() {
      bind(LocalStackContainer.class).toInstance(memoizedContainerSupplier.get());
    }
  }
}
