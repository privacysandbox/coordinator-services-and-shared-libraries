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

import com.google.inject.AbstractModule;

/**
 * Abstract module class to be implemented for the various {@code BlobStorageClient} implementations
 */
public abstract class BlobStorageClientModule extends AbstractModule {

  /** Returns a {@code BlobStorageClient} object for the storage provider implementation */
  public abstract Class<? extends BlobStorageClient> getBlobStorageClientImplementation();

  /**
   * Arbitrary configurations that can be done by the implementing class to support dependencies
   * that are specific to that implementation.
   */
  public void configureModule() {}

  /**
   * Configures injected dependencies for this module. Includes a binding for {@code
   * BlobStorageClient} in addition to any found in the {@code configureModule} method.
   */
  @Override
  protected final void configure() {
    bind(BlobStorageClient.class).to(getBlobStorageClientImplementation());
    configureModule();
  }
}
