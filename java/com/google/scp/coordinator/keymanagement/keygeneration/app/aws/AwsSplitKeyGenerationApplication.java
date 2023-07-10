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

package com.google.scp.coordinator.keymanagement.keygeneration.app.aws;

import com.beust.jcommander.JCommander;
import com.google.common.util.concurrent.ServiceManager;
import com.google.inject.Guice;
import com.google.inject.Injector;

/**
 * Application which subscribes to key generation events and generates key splits when it receives
 * the event.
 */
final class AwsSplitKeyGenerationApplication {

  /** Parses input arguments and starts listening for key generation event. */
  public static void main(String[] args) {
    AwsSplitKeyGenerationArgs params = new AwsSplitKeyGenerationArgs();
    JCommander.newBuilder().addObject(params).build().parse(args);

    Injector injector = Guice.createInjector(new AwsSplitKeyGenerationModule(params));
    ServiceManager serviceManager = injector.getInstance(ServiceManager.class);
    serviceManager.startAsync();
  }
}
