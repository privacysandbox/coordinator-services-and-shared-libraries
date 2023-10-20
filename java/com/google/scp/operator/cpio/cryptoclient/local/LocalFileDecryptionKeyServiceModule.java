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

package com.google.scp.operator.cpio.cryptoclient.local;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.BindingAnnotation;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyServiceModule;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/**
 * Guice Module for the local file implementation of {@link DecryptionKeyService} where key is
 * stored at {@link DecryptionKeyFilePath}.
 */
public final class LocalFileDecryptionKeyServiceModule extends DecryptionKeyServiceModule {

  @Override
  public Class<? extends DecryptionKeyService> getDecryptionKeyServiceImplementation() {
    return LocalFileDecryptionKeyService.class;
  }

  /**
   * Annotation for a local path to the decryption key used for {@link
   * LocalFileDecryptionKeyService}
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface DecryptionKeyFilePath {}
}
