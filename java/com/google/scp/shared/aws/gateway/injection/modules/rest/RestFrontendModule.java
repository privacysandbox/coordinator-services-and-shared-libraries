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

package com.google.scp.shared.aws.gateway.injection.modules.rest;

import com.google.auto.service.AutoService;
import com.google.scp.shared.aws.gateway.injection.modules.frontend.BaseFrontendModule;
import com.google.scp.shared.aws.gateway.serialization.ObjectMapperSerializerFacade;
import com.google.scp.shared.aws.gateway.serialization.SerializerFacade;

/** Configures all the modules required to run the frontend service. */
@AutoService(BaseFrontendModule.class)
public final class RestFrontendModule extends BaseFrontendModule {

  @Override
  public Class<? extends SerializerFacade> getSerializerImplementation() {
    return ObjectMapperSerializerFacade.class;
  }
}
