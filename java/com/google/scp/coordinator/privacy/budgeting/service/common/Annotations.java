/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.privacy.budgeting.service.common;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/** Guice annotations for dependency injection. */
public final class Annotations {

  private Annotations() {}

  /**
   * Annotation for providing an implementation of {@link
   * com.fasterxml.jackson.databind.ObjectMapper} that serializes and deserializes Immutable classes
   * from guava appropriately.
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PrivacyBudgetObjectMapper {}

  /** Annotation for a privacy budgeting database. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PrivacyBudgetDatabase {}

  /** Annotation for a database client. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PrivacyBudgetDatabaseClient {}

  /** Annotation for a database table name. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PrivacyBudgetDatabaseTableName {}

  /** Annotation for the privacy budget limit that is allowed per privacy budget unit. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PrivacyBudgetLimit {}

  /**
   * Annotation for an offset to use with a request's privacy budget units' reporting windows to
   * determine whether the service will accept the request. The reporting window should be between
   * now() and now() - offset.
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface ReportingWindowOffset {}

  /**
   * Annotation for the buffer added on top of the privacy budget ttl that is used in the privacy
   * budget database.
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PrivacyBudgetTtlBuffer {}

  /** Annotation for a database table name. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface AdTechIdentityDatabaseTableName {}

  /** Annotation for a database client. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface AdTechIdentityDatabaseClient {}

  /** Annotation for flag to indicate if application auth checks should be enabled. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface RequestAuthEnabled {}
}
