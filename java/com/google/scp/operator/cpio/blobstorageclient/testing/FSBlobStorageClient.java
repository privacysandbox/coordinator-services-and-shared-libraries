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

package com.google.scp.operator.cpio.blobstorageclient.testing;

import com.google.common.collect.ImmutableList;
import com.google.scp.operator.cpio.blobstorageclient.BlobStorageClient;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation;
import com.google.scp.operator.cpio.blobstorageclient.model.DataLocation.BlobStoreDataLocation;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.FileSystem;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Optional;
import javax.inject.Inject;

/** {@code BlobStorageClient} fake for testing, using Jimfs. */
public class FSBlobStorageClient implements BlobStorageClient {

  private FileSystem fileSystem;
  private volatile Path lastWrittenFile = null;

  @Inject
  public FSBlobStorageClient(FileSystem fileSystem) {
    this.fileSystem = fileSystem;
  }

  @Override
  public InputStream getBlob(DataLocation location) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      return Files.newInputStream(fileSystem.getPath(blobLocation.bucket(), blobLocation.key()));
    } catch (IOException e) {
      throw new BlobStorageClientException(e);
    }
  }

  @Override
  public InputStream getBlob(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    return getBlob(location);
  }

  @Override
  public InputStream getBlobRange(DataLocation location, int startOffset, int length)
      throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();

    Long fileSize = this.getBlobSize(location);
    int rangeEnd = startOffset + length - 1;
    if (startOffset < 0 || length < 1 || rangeEnd >= fileSize) {
      throw new BlobStorageClientException(
          "Specified byte range outside of blob size: " + fileSize);
    }

    try (InputStream fis =
        Files.newInputStream(fileSystem.getPath(blobLocation.bucket(), blobLocation.key()))) {
      byte[] buff = new byte[length];
      fis.skip(startOffset);
      fis.read(buff);

      return new ByteArrayInputStream(buff);
    } catch (IOException e) {
      throw new BlobStorageClientException(e);
    }
  }

  @Override
  public Long getBlobSize(DataLocation location) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      Path filePath = fileSystem.getPath(blobLocation.bucket(), blobLocation.key());
      return Files.size(filePath);
    } catch (IOException e) {
      throw new BlobStorageClientException(e);
    }
  }

  @Override
  public Long getBlobSize(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      Path filePath = fileSystem.getPath(blobLocation.bucket(), blobLocation.key());
      return Files.size(filePath);
    } catch (IOException e) {
      throw new BlobStorageClientException(e);
    }
  }

  @Override
  public void putBlob(DataLocation location, Path filePath) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      Path bucketDirPath = fileSystem.getPath("/", blobLocation.bucket());
      String blobKey = blobLocation.key();

      // Include prefixes from key when creating the directory, e.g. /bucket/prefixA/prefixB/
      int endOfPrefixesIndex = blobKey.lastIndexOf('/');
      String prefixes = (endOfPrefixesIndex >= 0) ? blobKey.substring(0, endOfPrefixesIndex) : "";
      Path createDirPath = bucketDirPath.resolve(prefixes);

      Files.createDirectories(createDirPath);
      lastWrittenFile = Files.copy(filePath, bucketDirPath.resolve(blobKey));
    } catch (IOException e) {
      throw new BlobStorageClientException(e);
    }
  }

  @Override
  public void putBlob(DataLocation location, Path filePath, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    putBlob(location, filePath);
  }

  @Override
  public ImmutableList<String> listBlobs(DataLocation location) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      if (!Files.exists(fileSystem.getPath(blobLocation.bucket()))) {
        throw new BlobStorageClientException("Bucket does not exist.");
      }

      Path fullPathWithPrefixes = Path.of(blobLocation.bucket()).resolve(blobLocation.key());

      // remove the last prefix, if there are any prefixes; everything else must be a folder
      String startingSearchPath =
          fullPathWithPrefixes.toString().contains("/")
              ? fullPathWithPrefixes.getParent().toString()
              : fullPathWithPrefixes.toString();

      // Use of String::startsWith is important, it has different behavior from Path::startsWith
      return Files.find(
              fileSystem.getPath(startingSearchPath),
              Integer.MAX_VALUE,
              (path, attrs) ->
                  (attrs.isRegularFile() || attrs.isSymbolicLink())
                      && path.toString().startsWith(fullPathWithPrefixes.toString()))
          // entire path after the bucket is part of the key.
          .map(path -> fileSystem.getPath(blobLocation.bucket()).relativize(path))
          .map(Path::toString)
          .collect(ImmutableList.toImmutableList());
    } catch (IOException e) {
      throw new BlobStorageClientException(e);
    }
  }

  @Override
  public ImmutableList<String> listBlobs(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    return listBlobs(location);
  }

  @Override
  public void deleteBlob(DataLocation location) throws BlobStorageClientException {
    BlobStoreDataLocation blobLocation = location.blobStoreDataLocation();
    try {
      Files.delete(fileSystem.getPath(blobLocation.bucket(), blobLocation.key()));
    } catch (IOException e) {
      throw new BlobStorageClientException(e);
    }
  }

  @Override
  public void deleteBlob(DataLocation location, Optional<String> accountIdentity)
      throws BlobStorageClientException {
    deleteBlob(location);
  }

  public synchronized Path getLastWrittenFile() {
    return lastWrittenFile;
  }
}
