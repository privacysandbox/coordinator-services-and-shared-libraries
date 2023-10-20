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

package com.google.scp.operator.cpio.notificationclient.aws;

import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.notificationclient.NotificationClient;
import com.google.scp.operator.cpio.notificationclient.NotificationClientModule;
import com.google.scp.shared.clients.configclient.Annotations.ApplicationRegionBinding;
import java.io.IOException;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.core.client.config.ClientOverrideConfiguration;
import software.amazon.awssdk.core.retry.RetryPolicy;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.sns.SnsClient;
import software.amazon.awssdk.services.sns.SnsClientBuilder;

/** Defines dependencies to be used in the AWS Notification Client. */
public final class AwsNotificationClientModule extends NotificationClientModule {

  /**
   * Gets the GCP implementation of {@code NotificationClient}.
   *
   * @return GcpNotificationClient
   */
  @Override
  public Class<? extends NotificationClient> getNotificationClientImpl() {
    return AwsNotificationClient.class;
  }

  /** Provides a singleton instance of the {@code SnsClient} class. */
  @Provides
  @Singleton
  SnsClient provideSnsClient(
      AwsCredentialsProvider credentials,
      SdkHttpClient httpClient,
      RetryPolicy retryPolicy,
      @ApplicationRegionBinding String region)
      throws IOException {
    SnsClientBuilder clientBuilder =
        SnsClient.builder()
            .credentialsProvider(credentials)
            .httpClient(httpClient)
            .overrideConfiguration(
                ClientOverrideConfiguration.builder().retryPolicy(retryPolicy).build())
            .region(Region.of(region));

    return clientBuilder.build();
  }
}
