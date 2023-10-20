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

package com.google.scp.operator.cpio.blobstorageclient.model;

import com.google.auto.value.AutoOneOf;
import com.google.auto.value.AutoValue;
import java.nio.file.Path;

/** Location of encrypted reports. */
@AutoOneOf(DataLocation.Kind.class)
public abstract class DataLocation {

  /** Returns a {@code DataLocation} from the given local path. */
  public static DataLocation ofLocalNioPath(Path localPath) {
    return AutoOneOf_DataLocation.localNioPath(localPath);
  }

  /** Returns a {@code DataLocation} from the given blob store location. */
  public static DataLocation ofBlobStoreDataLocation(BlobStoreDataLocation blobStoreDataLocation) {
    return AutoOneOf_DataLocation.blobStoreDataLocation(blobStoreDataLocation);
  }

  /** Returns the local path, if provided. */
  public abstract Path localNioPath();

  /** Returns the blob store location, if provided. */
  public abstract BlobStoreDataLocation blobStoreDataLocation();

  /** Returns the {@code Kind} for the currently stored data location. */
  public abstract Kind getKind();

  /** The kinds of data locations that can be stored. */
  public enum Kind {
    LOCAL_NIO_PATH,
    BLOB_STORE_DATA_LOCATION
  }

  /** Contains info to access encrypted reports file from blob storage. */
  @AutoValue
  public abstract static class BlobStoreDataLocation {
    /** Creates an instance of the {@code BlobStoreDataLocation} class. */
    public static BlobStoreDataLocation create(String bucket, String key) {
      return new AutoValue_DataLocation_BlobStoreDataLocation(bucket, key);
    }

    /** Bucket for a blob storage data location. */
    public abstract String bucket();

    /** Key for a blob storage data location. */
    public abstract String key();
  }
}
