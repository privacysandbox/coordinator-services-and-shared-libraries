package com.google.scp.operator.cpio.blobstorageclient.gcp;

import static com.google.common.truth.Truth.assertThat;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.cloud.storage.Bucket;
import com.google.cloud.storage.BucketInfo;
import com.google.cloud.storage.Storage;
import com.google.common.collect.ImmutableList;
import com.google.common.hash.HashCode;
import com.google.common.hash.HashFunction;
import com.google.common.hash.Hashing;
import com.google.common.io.ByteStreams;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.BlobStorageClientException;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import com.google.scp.shared.testutils.gcp.LocalGcsContainerTestModule;
import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.file.Files;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class GcsBlobStorageClientTest {

  @Rule public Acai acai = new Acai(TestEnv.class);

  @Rule public TemporaryFolder folder = new TemporaryFolder();

  @Inject private Storage gcsStorage;

  @Inject private GcsBlobStorageClient gcsBlobStorageClient;

  private static final String TEST_STRING = "GCS integration test with testcontainers";

  private static final String GCP_BUCKET = "gcp_bucket";

  private File file;

  @Before
  public void setUp() throws IOException {
    // Create text file to upload.
    try {
      file = folder.newFile("file.txt");
      FileWriter writer = new FileWriter(file, Charset.forName("UTF-8"));
      writer.write(TEST_STRING);
      writer.close();
    } catch (IOException e) {
      e.printStackTrace();
      throw new IOException("Could not create test file in temp dir.");
    }
    // Need to create the bucket ahead of time.
    gcsStorage.create(BucketInfo.newBuilder(GCP_BUCKET).build());
  }

  @After
  public void cleanUp() throws BlobStorageClientException {
    ImmutableList<String> keys =
        gcsBlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(
                BlobStoreDataLocation.create(GCP_BUCKET, /* withPrefix= */ "")));
    for (String key : keys) {
      gcsBlobStorageClient.deleteBlob(
          DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, key)));
    }
    Bucket bucket = gcsStorage.get(GCP_BUCKET);
    bucket.delete();
  }

  @Test
  public void getBlob_consistentObjectRetrieval() throws IOException, BlobStorageClientException {
    HashFunction hashFunction = Hashing.sha512();
    HashCode hashPreUpload = hashFunction.hashString(TEST_STRING, UTF_8);
    ByteArrayOutputStream downloadedBytesStream = new ByteArrayOutputStream(64);

    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, "keyname"));
    gcsBlobStorageClient.putBlob(location, file.toPath());
    InputStream download = gcsBlobStorageClient.getBlob(location);
    BufferedInputStream input = new BufferedInputStream(download);
    InputStream sizedInputStream = ByteStreams.limit(input, 64);
    ByteStreams.copy(sizedInputStream, downloadedBytesStream);
    downloadedBytesStream.flush();
    downloadedBytesStream.close();
    ByteBuffer downloadedBytesBuffer = ByteBuffer.wrap(downloadedBytesStream.toByteArray());
    HashCode hashPostUpload = hashFunction.hashBytes(downloadedBytesBuffer);
    assertThat(hashPreUpload).isEqualTo(hashPostUpload);
  }

  @Test
  public void getBlobRange_returnsSpecifiedBytes() throws Exception {
    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, "keyname"));
    gcsBlobStorageClient.putBlob(location, file.toPath());

    InputStream blobRange = gcsBlobStorageClient.getBlobRange(location, 0, 15);
    String downloadedMessage = new String(blobRange.readAllBytes());
    assertThat(downloadedMessage).isEqualTo("GCS integration");
  }

  @Test
  public void getBlobRange_rangeTowardsTheEndReturnsSpecifiedBytes() throws Exception {
    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, "keyname"));
    gcsBlobStorageClient.putBlob(location, file.toPath());

    InputStream blobRange = gcsBlobStorageClient.getBlobRange(location, 26, 14);
    String downloadedMessage = new String(blobRange.readAllBytes());
    assertThat(downloadedMessage).isEqualTo("testcontainers");
  }

  @Test
  public void getBlobRange_startOffsetLessThanZeroThrowsException() throws Exception {
    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, "keyname"));
    gcsBlobStorageClient.putBlob(location, file.toPath());

    assertThrows(
        BlobStorageClientException.class, () -> gcsBlobStorageClient.getBlobRange(location, -1, 4));
  }

  @Test
  public void getBlobRange_lengthGreaterThanBlobLengthThrowsException() throws Exception {
    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, "keyname"));
    gcsBlobStorageClient.putBlob(location, file.toPath());

    assertThrows(
        BlobStorageClientException.class,
        () -> gcsBlobStorageClient.getBlobRange(location, 0, 100));
  }

  @Test
  public void getBlobRange_lengthLessThanOneThrowsException() throws Exception {
    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, "keyname"));
    gcsBlobStorageClient.putBlob(location, file.toPath());

    assertThrows(
        BlobStorageClientException.class, () -> gcsBlobStorageClient.getBlobRange(location, 0, 0));
  }

  @Test
  public void getBlobRange_exceptionOnMissingObject() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            gcsBlobStorageClient.getBlobRange(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create(GCP_BUCKET, "NonExistentKey")),
                0,
                0));
  }

  @Test
  public void getBlob_exceptionOnMissingObject() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            gcsBlobStorageClient.getBlob(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create(GCP_BUCKET, "NonExistentKey"))));
  }

  @Test
  public void getBlob_NoSuchBucketException() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            gcsBlobStorageClient.getBlob(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create("no-such-bucket", ""))));
  }

  @Test
  public void getBlobSize_matchesFileSize() throws Exception {
    Long fileSize = Files.size(file.toPath());
    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, "keyname"));
    gcsBlobStorageClient.putBlob(location, file.toPath());

    Long gcsBlobSize = gcsBlobStorageClient.getBlobSize(location);

    assertThat(gcsBlobSize).isEqualTo(fileSize);
  }

  @Test
  public void getBlobSize_exceptionOnMissingObject() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            gcsBlobStorageClient.getBlobSize(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create(GCP_BUCKET, "NonExistentKey"))));
  }

  @Test
  public void getBlobSize_NoSuchBucketException() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            gcsBlobStorageClient.getBlobSize(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create("no-such-bucket", ""))));
  }

  @Test
  public void deleteBlob_successfulDeleteThenThrowsOnGet() throws Exception {
    DataLocation location =
        DataLocation.ofBlobStoreDataLocation(
            BlobStoreDataLocation.create(GCP_BUCKET, "toBeDeleted"));
    gcsBlobStorageClient.putBlob(location, file.toPath());
    gcsBlobStorageClient.deleteBlob(location);

    assertThrows(BlobStorageClientException.class, () -> gcsBlobStorageClient.getBlob(location));
  }

  @Test
  public void putBlob_NoSuchBucketException() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            gcsBlobStorageClient.putBlob(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create("no-such-bucket", /* withPrefix= */ "")),
                file.toPath()));
  }

  @Test
  public void listBlobs_consistentKeyRetrieval() throws Exception {
    String keyName = "keyName";
    gcsBlobStorageClient.putBlob(
        DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, keyName)),
        file.toPath());

    ImmutableList<String> keys =
        gcsBlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(
                BlobStoreDataLocation.create(GCP_BUCKET, /* withPrefix= */ "")));

    assertThat(keys).hasSize(1);
    assertThat(keys).containsExactly(keyName);
  }

  @Test
  public void listBlobs_listAllKeys() throws Exception {
    ImmutableList<String> keys = ImmutableList.of("key1", "key2", "1key");
    ImmutableList<String> expectedKeys = ImmutableList.of("key1", "key2", "1key");
    String prefix = "";
    for (String key : keys) {
      gcsBlobStorageClient.putBlob(
          DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, key)),
          file.toPath());
    }

    ImmutableList<String> filteredKeys =
        gcsBlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, prefix)));

    assertThat(filteredKeys).containsExactlyElementsIn(expectedKeys);
  }

  @Test
  public void listBlobs_listOneKeyOutOfMany() throws Exception {
    ImmutableList<String> keys = ImmutableList.of("key1", "key2", "1key");
    ImmutableList<String> expectedKeys = ImmutableList.of("key1");
    String prefix = "key1";
    for (String key : keys) {
      gcsBlobStorageClient.putBlob(
          DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, key)),
          file.toPath());
    }

    ImmutableList<String> filteredKeys =
        gcsBlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, prefix)));

    assertThat(filteredKeys).containsExactlyElementsIn(expectedKeys);
  }

  @Test
  public void listBlobs_listKeysWithPartialPrefixMatch() throws Exception {
    ImmutableList<String> keys = ImmutableList.of("key1", "key2", "1key");
    ImmutableList<String> expectedKeys = ImmutableList.of("key1", "key2");
    String prefix = "key";
    for (String key : keys) {
      gcsBlobStorageClient.putBlob(
          DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, key)),
          file.toPath());
    }

    ImmutableList<String> filteredKeys =
        gcsBlobStorageClient.listBlobs(
            DataLocation.ofBlobStoreDataLocation(BlobStoreDataLocation.create(GCP_BUCKET, prefix)));

    assertThat(filteredKeys).containsExactlyElementsIn(expectedKeys);
  }

  @Test
  public void listBlobs_NoSuchBucketException() {
    assertThrows(
        BlobStorageClientException.class,
        () ->
            gcsBlobStorageClient.listBlobs(
                DataLocation.ofBlobStoreDataLocation(
                    BlobStoreDataLocation.create("no-such-bucket", /* withPrefix= */ ""))));
  }

  /** The main guice module used for this test. */
  public static final class TestEnv extends AbstractModule {

    @Override
    public void configure() {
      install(new LocalGcsContainerTestModule());
    }

    @Provides
    @Inject
    @Singleton
    public GcsBlobStorageClient providesGcsBlobStorageClient(Storage gcsStorage) {
      return new GcsBlobStorageClient(gcsStorage);
    }
  }
}
