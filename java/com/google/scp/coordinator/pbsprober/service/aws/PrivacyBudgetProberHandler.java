/*
 * Copyright 2023 Google LLC
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

package com.google.scp.coordinator.pbsprober.service.aws;

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.RequestHandler;
import com.amazonaws.services.lambda.runtime.events.ScheduledEvent;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.scp.coordinator.pbsprober.RandomPrivacyBudgetConsumer;
import com.google.scp.coordinator.pbsprober.SystemEnvironmentVariableModule;
import com.google.scp.coordinator.pbsprober.model.ProbeResponse;

/**
 * AWS Lambda Handler for privacy budget prober service. When this handler receives a request from
 * client, it will send a valid transaction request to consume a random privacy budget key to
 * privacy budget server.
 */
public final class PrivacyBudgetProberHandler
    implements RequestHandler<ScheduledEvent, ProbeResponse> {
  private final RandomPrivacyBudgetConsumer consumer;

  public PrivacyBudgetProberHandler() {
    Injector injector =
        Guice.createInjector(
            new SystemEnvironmentVariableModule(),
            new AwsSystemEnvironmentVariableModule(),
            new AwsPrivacyBudgetProberModule());
    this.consumer = injector.getInstance(RandomPrivacyBudgetConsumer.class);
  }

  @Override
  public ProbeResponse handleRequest(ScheduledEvent request, Context context) {
    try {
      consumer.consumeBudget();
    } catch (Exception e) {
      // Wraps the Exception to a new RuntimeException since the overriden handleRequest method does
      // not throw an Exception.
      throw new RuntimeException(e);
    }
    return ProbeResponse.builder().build();
  }
}
