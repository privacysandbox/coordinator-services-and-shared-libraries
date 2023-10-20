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
