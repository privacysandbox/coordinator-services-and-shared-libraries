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

package com.google.scp.shared.aws.gateway.injection.factories;

import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.operator.shared.injection.factories.ModuleFactory;
import com.google.scp.shared.aws.gateway.injection.modules.frontend.BaseFrontendModule;
import com.google.scp.shared.aws.gateway.serialization.SerializerFacade;

/** Exposes the frontend related services needed by lambda functions. */
public final class FrontendServicesFactory {

  private static Injector injector =
      Guice.createInjector(ModuleFactory.getModule(BaseFrontendModule.class));

  /** Gets an instance of the {@code JsonSerializerFacade} class. */
  public static SerializerFacade getJsonSerializer() {
    return injector.getInstance(SerializerFacade.class);
  }

  /** Gets an instance of the {@code Injector} class being used to return services. */
  public static Injector getInjector() {
    return injector;
  }

  /** Sets the instance of the {@code Injector} class used to return services. */
  public static void setInjector(Injector injector) {
    FrontendServicesFactory.injector = injector;
  }
}
