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

package com.google.scp.operator.cpio.configclient;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Annotations used for client configuration bindings. */
public final class Annotations {

  /**
   * Cloud Region where key management (including KMS keys) and privacy budget services are hosted.
   * Provided as a {@link software.amazon.awssdk.regions.Region} for compatibility reasons.
   *
   * <p><b>Internally Bound<b> annotation, external consumers of the CPIO library should use {@link
   * CoordinatorARegionBindingOverride} to set this value.
   *
   * <p>Used for signing requests to coordinator services and creating KMS clients configured for
   * the correct region when within the Enclave.
   *
   * <p>In the future, coordinator services may be hosted in multiple regions at which point this
   * configuration value may become obsolete.
   *
   * <p>TODO(b/221319893): The interface provided by this annotation should match that of {@link
   * AdtechRegionBinding}
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorARegionBinding {};

  /**
   * Cloud Region where key management (including KMS keys) and privacy budget services are hosted.
   * Provided as a {@link software.amazon.awssdk.regions.Region} for compatibility reasons.
   *
   * <p><b>Internally Bound<b> annotation, external consumers of the CPIO library should use {@link
   * CoordinatorBRegionBindingOverride} to set this value.
   *
   * <p>Used for signing requests to coordinator services and creating KMS clients configured for
   * the correct region when within the Enclave.
   *
   * <p>In the future, coordinator services may be hosted in multiple regions at which point this
   * configuration value may become obsolete.
   *
   * <p>TODO(b/221319893): The interface provided by this annotation should match that of {@link
   * AdtechRegionBinding}
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorBRegionBinding {};

  /**
   * Cloud Region where key management (including KMS keys) and privacy budget services are hosted
   * by coordinator A. Must be provided as a string e.g. "us-west-2"
   *
   * <p>TODO(b/221319893): Provide standardized interface for providing these values.
   *
   * @see CoordinatorARegionBinding
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorARegionBindingOverride {};

  /**
   * Cloud Region where key management (including KMS keys) and privacy budget services are hosted
   * by coordinator B. Must be provided as a string e.g. "us-west-2"
   *
   * <p>TODO(b/221319893): Provide standardized interface for providing these values.
   *
   * @see CoordinatorARegionBinding
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface CoordinatorBRegionBindingOverride {}

  /** HTTP client with credentials to access Coordinator A's encryption key vending service. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorAHttpClient {}

  /** HTTP client with credentials to access Coordinator B's encryption key vending service. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorBHttpClient {}
}
