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

import com.amazonaws.services.lambda.runtime.Context;
import com.amazonaws.services.lambda.runtime.RequestHandler;
import com.amazonaws.services.lambda.runtime.events.ScheduledEvent;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Key;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationKeyCount;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationTtlInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.Annotations.KeyGenerationValidityInDays;
import com.google.scp.coordinator.keymanagement.keygeneration.tasks.common.CreateKeysTask;
import com.google.scp.shared.api.exception.ServiceException;

/**
 * AWS Lambda function for generating asymmetric keys and storing them in DynamoDB.
 *
 * <p>Each time the handler is invoked a new key is generated, the new key is encrypted using an AWS
 * KMS symmetric key, and the encrypted key is stored in DynamoDB.
 *
 * <p>Intended to be used with CloudWatchEvents to periodically invoke this function.
 *
 * @deprecated Single-party key features are deprecated. Pending removal b/282204533.
 */
@Deprecated
public final class KeyGenerationLambda implements RequestHandler<ScheduledEvent, String> {

  private final Integer keyGenerationKeyCount;
  private final Integer keyGenerationValidityInDays;
  private final Integer keyGenerationTtlInDays;
  private final CreateKeysTask createKeysTask;

  /** Creates handler with correct defaults for lambda deployment. */
  public KeyGenerationLambda() {
    Injector injector = Guice.createInjector(new AwsKeyGenerationModule());
    this.keyGenerationKeyCount =
        injector.getInstance(Key.get(Integer.class, KeyGenerationKeyCount.class));
    this.keyGenerationValidityInDays =
        injector.getInstance(Key.get(Integer.class, KeyGenerationValidityInDays.class));
    this.keyGenerationTtlInDays =
        injector.getInstance(Key.get(Integer.class, KeyGenerationTtlInDays.class));
    this.createKeysTask = injector.getInstance(CreateKeysTask.class);
  }

  /** Test-only constructor */
  public KeyGenerationLambda(
      Integer keyGenerationKeyCount,
      Integer keyGenerationValidityInDays,
      Integer keyGenerationTtlInDays,
      CreateKeysTask createKeysTask) {
    this.keyGenerationKeyCount = keyGenerationKeyCount;
    this.keyGenerationValidityInDays = keyGenerationValidityInDays;
    this.keyGenerationTtlInDays = keyGenerationTtlInDays;
    this.createKeysTask = createKeysTask;
  }

  @Override
  public String handleRequest(ScheduledEvent event, Context context) {
    try {
      createKeysTask.replaceExpiringKeys(
          /* numDesiredKeys= */ keyGenerationKeyCount,
          /* validityInDays= */ keyGenerationValidityInDays,
          /* ttlInDays= */ keyGenerationTtlInDays);
    } catch (ServiceException e) {
      throw new IllegalStateException(e);
    }

    return "Key Generation Success!";
  }
}
