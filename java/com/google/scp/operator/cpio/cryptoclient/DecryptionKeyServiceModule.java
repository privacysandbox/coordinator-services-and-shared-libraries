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

package com.google.scp.operator.cpio.cryptoclient;

import com.google.inject.AbstractModule;

/**
 * Abstract guice module to be implemented by modules for each implementation of {@link
 * DecryptionKeyService}.
 */
public abstract class DecryptionKeyServiceModule extends AbstractModule {
  /**
   * Returns the {@code Class} for the {@code DecryptionKeyService} implementation the module will
   * use.
   */
  public abstract Class<? extends DecryptionKeyService> getDecryptionKeyServiceImplementation();

  /**
   * Arbitrary configurations that can be done by the child class to support dependencies that are
   * specific to that implementation.
   */
  public void configureModule() {}

  /**
   * Configures injected dependencies for this module. Includes a binding for {@code
   * DecryptionKeyService} in addition to any bindings in the {@code customConfigure} method.
   */
  @Override
  protected final void configure() {
    bind(DecryptionKeyService.class).to(getDecryptionKeyServiceImplementation());
    configureModule();
  }
}
