/*
 * Copyright 2025 Google LLC
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

package com.google.scp.shared.testutils.gcp;

import static com.google.scp.shared.gcp.Constants.GCP_TEST_PROJECT_ID;

import com.google.cloud.storage.Storage;
import com.google.cloud.storage.StorageOptions;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.inject.Provides;
import com.google.inject.Singleton;

/** Module that provides the GcsBlobStorageClient for testing. */
public class LocalGcsContainerTestModule extends AbstractModule {

  @Provides
  @Singleton
  public LocalGcsContainer providesLocalGcsContainer() {
    return new LocalGcsContainer();
  }

  @Provides
  @Inject
  @Singleton
  public Storage providesStorage(LocalGcsContainer localGcsContainer) {
    return StorageOptions.newBuilder()
        .setProjectId(GCP_TEST_PROJECT_ID)
        .setHost(localGcsContainer.getContainerIpAddress())
        .build()
        .getService();
  }
}
