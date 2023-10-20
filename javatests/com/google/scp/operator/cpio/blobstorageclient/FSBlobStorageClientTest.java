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

package com.google.scp.operator.cpio.blobstorageclient;

import static com.google.common.truth.Truth.assertThat;
import static com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.getDataLocation;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertThrows;

import com.google.acai.Acai;
import com.google.common.collect.ImmutableList;
import com.google.common.jimfs.Configuration;
import com.google.common.jimfs.Jimfs;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient.BlobStorageClientException;
import com.google.scp.operator.cpio.blobstorageclient.testing.FSBlobStorageClient;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.FileSystem;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.List;
import java.util.stream.Collectors;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class FSBlobStorageClientTest {
  private static Path bucketPath;
  @Inject FSBlobStorageClient fsBlobStorageClient;
  private static FileSystem fileSystem =
      Jimfs.newFileSystem(Configuration.unix().toBuilder().setWorkingDirectory("/").build());
  private static String TEST_MESSAGE = "testing 123";
  private static String createDirPrefix = "a/b/";

  @Rule public final Acai acai = new Acai(TestEnv.class);

  @Before
  public void setUp() throws IOException {
    bucketPath = fileSystem.getPath("/testbucket");
    Files.createDirectories(bucketPath);
  }

  @After
  public void cleanUp() throws IOException {
    List<Path> cleanupPaths =
        Files.walk(bucketPath).sorted(Comparator.reverseOrder()).collect(Collectors.toList());

    for (Path path : cleanupPaths) {
      Files.deleteIfExists(path);
    }
  }

  @Test
  public void getBlobReturnsExpected() throws IOException, BlobStorageClientException {
    String getBlobKey = "get-blob";
    Path blobPath = bucketPath.resolve(getBlobKey);
    Files.createFile(blobPath);
    Files.writeString(blobPath, TEST_MESSAGE);

    InputStream blobStream =
        fsBlobStorageClient.getBlob(getDataLocation("/testbucket", getBlobKey));
    String blobData = new String(blobStream.readAllBytes(), UTF_8);

    assertThat(blobData).isEqualTo(TEST_MESSAGE);
  }

  @Test
  public void putBlobWritesBlob() throws IOException, BlobStorageClientException {
    Path inputPath = bucketPath.resolve("put-blob-input");
    String putBlobKey = "put-blob-output";
    Files.writeString(inputPath, TEST_MESSAGE);
    Path outputPath = bucketPath.resolve(putBlobKey);

    fsBlobStorageClient.putBlob(getDataLocation("testbucket", putBlobKey), inputPath);

    assertThat(Files.readString(outputPath)).isEqualTo(TEST_MESSAGE);
  }

  @Test
  public void putBlobWritesBlob_newNestedDirectory()
      throws IOException, BlobStorageClientException {
    Path inputPath = bucketPath.resolve("put-blob-input");
    String putBlobKey = createDirPrefix + "put-blob-output";
    Files.writeString(inputPath, TEST_MESSAGE);
    Path outputPath = bucketPath.resolve(putBlobKey);

    fsBlobStorageClient.putBlob(getDataLocation("testbucket", putBlobKey), inputPath);

    assertThat(Files.readString(outputPath)).isEqualTo(TEST_MESSAGE);
  }

  @Test
  public void deleteBlobDeletesBlob() throws IOException, BlobStorageClientException {
    String deleteBlobKey = "delete-blob";
    Path inputPath = bucketPath.resolve(deleteBlobKey);
    Files.writeString(inputPath, TEST_MESSAGE);

    fsBlobStorageClient.deleteBlob(getDataLocation("testbucket", deleteBlobKey));

    assertThat(Files.exists(inputPath)).isFalse();
  }

  @Test
  public void listBlobListsBlobs_emptyKeyReturnsAll()
      throws IOException, BlobStorageClientException {
    Path blob1 = bucketPath.resolve("blob-1");
    Path blob2 = bucketPath.resolve("blob-2");
    Files.createFile(blob1);
    Files.createFile(blob2);

    ImmutableList<String> blobs = fsBlobStorageClient.listBlobs(getDataLocation("testbucket", ""));

    assertThat(blobs)
        .containsExactlyElementsIn(
            ImmutableList.of(blob1.getFileName().toString(), blob2.getFileName().toString()));
  }

  @Test
  public void listBlobListsBlobs_returnExactMatch() throws IOException, BlobStorageClientException {
    String targetBlobString = "blob-1";
    Path blob1 = bucketPath.resolve(targetBlobString);
    Path blob2 = bucketPath.resolve("blob-2");
    Path blob3 = bucketPath.resolve("bblob-1");
    Files.createFile(blob1);
    Files.createFile(blob2);
    Files.createFile(blob3);

    ImmutableList<String> blobs =
        fsBlobStorageClient.listBlobs(getDataLocation("testbucket", targetBlobString));

    assertThat(blobs).containsExactlyElementsIn(ImmutableList.of(blob1.getFileName().toString()));
  }

  @Test
  public void listBlobListsBlobs_returnPartialFilenameMatch()
      throws IOException, BlobStorageClientException {
    String targetBlobString = "blob-1";
    Path blob1 = bucketPath.resolve(targetBlobString);
    Path blob2 = bucketPath.resolve("blob-2");
    Path blob3 = bucketPath.resolve("blob-11");
    Files.createFile(blob1);
    Files.createFile(blob2);
    Files.createFile(blob3);

    ImmutableList<String> blobs =
        fsBlobStorageClient.listBlobs(getDataLocation("testbucket", targetBlobString));

    assertThat(blobs)
        .containsExactlyElementsIn(
            ImmutableList.of(blob1.getFileName().toString(), blob3.getFileName().toString()));
  }

  @Test
  public void listBlobListsBlobs_returnExactMatchWithBucketPrefix()
      throws IOException, BlobStorageClientException {
    Path longBucketPath = bucketPath.resolve(createDirPrefix);
    Files.createDirectories(longBucketPath);
    String targetBlobString = "blob-1";
    Path blob1 = longBucketPath.resolve(targetBlobString);
    Path blob2 = longBucketPath.resolve("blob-2");
    Path blob3 = longBucketPath.resolve("bblob-1");
    Files.createFile(blob1);
    Files.createFile(blob2);
    Files.createFile(blob3);

    ImmutableList<String> blobs =
        fsBlobStorageClient.listBlobs(getDataLocation("testbucket/a/b", targetBlobString));

    assertThat(blobs).containsExactlyElementsIn(ImmutableList.of(blob1.getFileName().toString()));
  }

  @Test
  public void listBlobListsBlobs_returnExactMatchWithKeyPrefix()
      throws IOException, BlobStorageClientException {
    Files.createDirectories(bucketPath.resolve(createDirPrefix));
    String targetBlobString = createDirPrefix + "blob-1";
    Path blob1 = bucketPath.resolve(targetBlobString);
    Path blob2 = bucketPath.resolve(createDirPrefix + "blob-2");
    Path blob3 = bucketPath.resolve(createDirPrefix + "bblob-1");
    Files.createFile(blob1);
    Files.createFile(blob2);
    Files.createFile(blob3);

    ImmutableList<String> blobs =
        fsBlobStorageClient.listBlobs(getDataLocation("testbucket", targetBlobString));

    assertThat(blobs).containsExactlyElementsIn(ImmutableList.of(blob1.getFileName().toString()));
  }

  @Test
  public void listBlobListsBlobs_returnExactMatchWithCombinedPrefix()
      throws IOException, BlobStorageClientException {
    Files.createDirectories(bucketPath.resolve("a/b/"));
    Path longBucketPath = bucketPath.resolve("a/");
    String targetBlobString = "b/blob-1";
    Path blob1 = longBucketPath.resolve(targetBlobString);
    Path blob2 = longBucketPath.resolve("b/blob-2");
    Path blob3 = longBucketPath.resolve("b/bblob-1");
    Files.createFile(blob1);
    Files.createFile(blob2);
    Files.createFile(blob3);

    ImmutableList<String> blobs =
        fsBlobStorageClient.listBlobs(getDataLocation("testbucket/a", targetBlobString));

    assertThat(blobs).containsExactlyElementsIn(ImmutableList.of(blob1.getFileName().toString()));
  }

  @Test
  public void listBlobListsBlobs_trailingPrefixCanMatchFilesAndFolders()
      throws IOException, BlobStorageClientException {
    Files.createDirectories(bucketPath.resolve("blob-11/"));
    String targetPrefix = "blob-1";
    Path blob1 = bucketPath.resolve("blob-1");
    Path blob2 = bucketPath.resolve("blob-11/blob-2");
    Files.createFile(blob1);
    Files.createFile(blob2);

    ImmutableList<String> blobs =
        fsBlobStorageClient.listBlobs(getDataLocation("testbucket", targetPrefix));

    assertThat(blobs)
        .containsExactlyElementsIn(
            ImmutableList.of(blob1.getFileName().toString(), blob2.getFileName().toString()));
  }

  @Test
  public void listBlobListsBlobs_trailingPrefixCanMatchMultipleFilesInMultipleFolders()
      throws IOException, BlobStorageClientException {
    Files.createDirectories(bucketPath.resolve("blob-1/"));
    Files.createDirectories(bucketPath.resolve("blob-11/"));
    Files.createDirectories(bucketPath.resolve("blob-2/"));
    String targetPrefix = "blob-1";
    Path blob1 = bucketPath.resolve("blob-1/blob-2");
    Path blob2 = bucketPath.resolve("blob-1/blob-3");
    Path blob3 = bucketPath.resolve("blob-11/blob-4");
    Path blob4 = bucketPath.resolve("blob-2/blob-1");
    Files.createFile(blob1);
    Files.createFile(blob2);
    Files.createFile(blob3);
    Files.createFile(blob4);

    ImmutableList<String> blobs =
        fsBlobStorageClient.listBlobs(getDataLocation("testbucket", targetPrefix));

    assertThat(blobs)
        .containsExactlyElementsIn(
            ImmutableList.of(
                blob1.getFileName().toString(),
                blob2.getFileName().toString(),
                blob3.getFileName().toString()));
  }

  @Test
  public void listBlobs_nonexistentBucket() {
    assertThrows(
        BlobStorageClientException.class,
        () -> fsBlobStorageClient.listBlobs(getDataLocation("fakeBucket", "blob-1")));
  }

  public static final class TestEnv extends AbstractModule {
    @Override
    protected final void configure() {
      bind(BlobStorageClient.class).to(FSBlobStorageClient.class);
    }

    @Provides
    @Singleton
    FileSystem provideFileSystem() {
      return fileSystem;
    }
  }
}
