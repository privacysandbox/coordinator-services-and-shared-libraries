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

package com.google.scp.shared.clients.configclient.local;

import com.google.scp.shared.clients.configclient.ParameterClient;
import com.google.scp.shared.clients.configclient.ParameterModule;

/** Guice module for binding the AWS parameter client functionality */
public final class LocalParameterModule extends ParameterModule {

  @Override
  public Class<? extends ParameterClient> getParameterClientImpl() {
    return LocalParameterClient.class;
  }
}
