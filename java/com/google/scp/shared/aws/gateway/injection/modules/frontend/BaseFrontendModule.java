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

package com.google.scp.shared.aws.gateway.injection.modules.frontend;

import com.google.inject.AbstractModule;
import com.google.inject.TypeLiteral;
import com.google.inject.binder.AnnotatedBindingBuilder;
import com.google.scp.shared.aws.gateway.serialization.SerializerFacade;
import java.util.logging.Logger;

/** Allows swappable/discoverable frontend modules when an inheritor's package is referenced. */
public abstract class BaseFrontendModule extends AbstractModule {

  /** Returns a {@code Class} object for the JsonSerializer. */
  public abstract Class<? extends SerializerFacade> getSerializerImplementation();

  protected void configureModule() {}

  @Override
  protected void configure() {
    bind(SerializerFacade.class).to(getSerializerImplementation());
  }

  /** Allows binding for non-generic classes, and adds additional logging. */
  protected @Override <T> AnnotatedBindingBuilder<T> bind(Class<T> bind) {
    Logger.getLogger("").info("Binding - " + bind.getName());
    return super.bind(bind);
  }

  /** Allows binding for generic classes, and adds additional logging. */
  protected @Override <T> AnnotatedBindingBuilder<T> bind(TypeLiteral<T> bind) {
    Logger.getLogger("").info("Binding - " + bind.toString());
    return super.bind(bind);
  }
}
