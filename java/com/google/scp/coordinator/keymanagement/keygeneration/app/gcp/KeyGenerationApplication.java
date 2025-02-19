/*
 * Copyright 2025 Google LLC
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

package com.google.scp.coordinator.keymanagement.keygeneration.app.gcp;

import com.beust.jcommander.JCommander;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.coordinator.keymanagement.keygeneration.app.gcp.listener.PubSubListener;

/**
 * Application which subscribes to generateKeys event and generates keys when it receives the event.
 */
public final class KeyGenerationApplication {

  /** Parses input arguments and starts listening for key generation event. */
  public static void main(String[] args) {
    KeyGenerationArgs params = new KeyGenerationArgs();
    JCommander.newBuilder().addObject(params).build().parse(args);

    Injector injector = Guice.createInjector(new KeyGenerationModule(params));
    PubSubListener listener = injector.getInstance(PubSubListener.class);
    listener.start();
  }
}
