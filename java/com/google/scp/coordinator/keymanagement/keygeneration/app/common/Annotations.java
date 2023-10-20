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

package com.google.scp.coordinator.keymanagement.keygeneration.app.common;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/** Annotations for key generation services */
public final class Annotations {

  private Annotations() {}

  /** Number of keys to generate at a time */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface KeyGenerationKeyCount {}

  /** Number of days keys will be valid */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface KeyGenerationValidityInDays {}

  /** Time-to-Live after key creation for keys */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface KeyGenerationTtlInDays {}

  /** Base URL (e.g. `https://foo.com/v1`) where the key storage service is located. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface KeyStorageServiceBaseUrl {}

  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorBHttpClient {}

  /**
   * Provides an {@code Optional<String>} binding to a URL to use for accessing the getDataKey
   * endpoint if present. Used when an external limitation makes it difficult for CreateKey and
   * GetDataKey to share a base URL.
   *
   * <p>One example of this is LocalStack not allowing the deployment of ApiGatewayV1 routes
   * containing colons in its path segments.
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface GetDataKeyBaseUrlOverride {}

  /** Key storage service cloud function url */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface KeyStorageServiceCloudfunctionUrl {}

  /** Uri of KMS key */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface KmsKeyUri {}

  /** Uri of KMS key for peer coordinator */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PeerCoordinatorKmsKeyUri {}

  /** Service account of peer coordinator */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PeerCoordinatorServiceAccount {}

  /** Service account of peer coordinator */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PeerCoordinatorWipProvider {}

  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface KeyIdTypeName {}
}
