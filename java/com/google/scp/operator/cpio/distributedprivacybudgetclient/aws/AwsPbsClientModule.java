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
package com.google.scp.operator.cpio.distributedprivacybudgetclient.aws;

import com.google.common.collect.ImmutableList;
import com.google.inject.Provides;
import com.google.inject.Singleton;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorARegionBinding;
import com.google.scp.operator.cpio.configclient.Annotations.CoordinatorBRegionBinding;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorACredentialsProvider;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBCredentialsProvider;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.DistributedPrivacyBudgetClientModule;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClient;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.PrivacyBudgetClientImpl;
import com.google.scp.shared.api.util.HttpClientWithInterceptor;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.aws.util.AwsAuthTokenInterceptor;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialAccessKey;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialSecretKey;
import java.util.Optional;
import org.apache.hc.core5.http.HttpRequestInterceptor;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.regions.Region;

public class AwsPbsClientModule extends DistributedPrivacyBudgetClientModule {

  @Provides
  @Singleton
  public ImmutableList<PrivacyBudgetClient> privacyBudgetClients(
      @CoordinatorAPrivacyBudgetServiceBaseUrl String coordinatorAPrivacyBudgetServiceBaseUrl,
      @CoordinatorBPrivacyBudgetServiceBaseUrl String coordinatorBPrivacyBudgetServiceBaseUrl,
      @CoordinatorAPrivacyBudgetServiceAuthEndpoint
          String coordinatorAPrivacyBudgetServiceAuthEndpoint,
      @CoordinatorBPrivacyBudgetServiceAuthEndpoint
          String coordinatorBPrivacyBudgetServiceAuthEndpoint,
      @AwsCredentialAccessKey String accessKey,
      @AwsCredentialSecretKey String secretKey,
      @CoordinatorACredentialsProvider
          AwsSessionCredentialsProvider coordinatorACredentialsProvider,
      @CoordinatorBCredentialsProvider
          AwsSessionCredentialsProvider coordinatorBCredentialsProvider,
      @CoordinatorARegionBinding Region coordinatorARegion,
      @CoordinatorBRegionBinding Region coordinatorBRegion) {
    Optional<AwsCredentialsProvider> staticCredentialProvider = Optional.empty();
    if (!accessKey.isEmpty() && !secretKey.isEmpty()) {
      // Doesn't use STS, so requests can't be signed (no session token).
      // Required for e2e testing.
      staticCredentialProvider =
          Optional.of(
              StaticCredentialsProvider.create(AwsBasicCredentials.create(accessKey, secretKey)));
    }

    HttpRequestInterceptor coordinatorATokenInterceptor =
        new AwsAuthTokenInterceptor(
            coordinatorARegion,
            coordinatorAPrivacyBudgetServiceAuthEndpoint,
            staticCredentialProvider.orElse(coordinatorACredentialsProvider));
    HttpRequestInterceptor coordinatorBTokenInterceptor =
        new AwsAuthTokenInterceptor(
            coordinatorBRegion,
            coordinatorBPrivacyBudgetServiceAuthEndpoint,
            staticCredentialProvider.orElse(coordinatorBCredentialsProvider));

    HttpClientWithInterceptor coordinatorAAwsHttpClient =
        new HttpClientWithInterceptor(coordinatorATokenInterceptor);

    HttpClientWithInterceptor coordinatorBAwsHttpClient =
        new HttpClientWithInterceptor(coordinatorBTokenInterceptor);

    return ImmutableList.of(
        new PrivacyBudgetClientImpl(
            coordinatorAAwsHttpClient, coordinatorAPrivacyBudgetServiceBaseUrl),
        new PrivacyBudgetClientImpl(
            coordinatorBAwsHttpClient, coordinatorBPrivacyBudgetServiceBaseUrl));
  }
}
