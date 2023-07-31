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

package com.google.scp.shared.clients.configclient;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Annotations used for client configuration bindings. */
public final class Annotations {

  /**
   * Region used for this application. Provided as a string e.g. "us-west-2"
   *
   * <p><b>Internally Bound<b> annotation, external consumers of the core libraries should use
   * {@link ApplicationRegionBindingOverride}
   *
   * <p>TODO(b/221319893): Should the standard interface for this be a Region or a string?
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface ApplicationRegionBinding {};

  /**
   * Forces the value of {@link ApplicationRegionBinding} -- if empty ApplicationRegionBinding will
   * default to the region provided by the EC2 Metadata service. Provided as a string e.g.
   * "us-west-2"
   *
   * <p>TODO(b/221319893): Provide standardized interface for providing these values.
   */
  @BindingAnnotation
  @Target({ElementType.FIELD, ElementType.PARAMETER, ElementType.METHOD})
  @Retention(RetentionPolicy.RUNTIME)
  public @interface ApplicationRegionBindingOverride {};
}
