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

package com.google.scp.operator.cpio.jobclient.local;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.BindingAnnotation;
import com.google.scp.operator.cpio.jobclient.JobClient;
import com.google.scp.operator.cpio.jobclient.JobHandlerModule;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/** Guice module for binding the job client that provides a local file job */
public final class LocalFileJobHandlerModule extends JobHandlerModule {

  @Override
  public Class<? extends JobClient> getJobClientImpl() {
    return LocalFileJobClient.class;
  }

  /** Annotation for binding the local file job handler path. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface LocalFileJobHandlerPath {}

  /** Annotation for binding the local file job handler result path. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface LocalFileJobHandlerResultPath {}

  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface LocalFileJobParameters {}
}
