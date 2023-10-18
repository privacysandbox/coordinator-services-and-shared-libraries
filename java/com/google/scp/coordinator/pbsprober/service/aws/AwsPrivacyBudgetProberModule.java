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

import static com.google.scp.coordinator.pbsprober.Annotations.CoordinatorARoleArn;
import static com.google.scp.coordinator.pbsprober.Annotations.CoordinatorBRoleArn;

import com.google.inject.AbstractModule;
import com.google.inject.Provides;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorACredentialsProvider;
import com.google.scp.operator.cpio.configclient.aws.Annotations.CoordinatorBCredentialsProvider;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionPhaseManager;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.TransactionPhaseManagerImpl;
import com.google.scp.operator.cpio.distributedprivacybudgetclient.aws.AwsPbsClientModule;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import com.google.scp.shared.aws.credsprovider.StsAwsSessionCredentialsProvider;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialAccessKey;
import com.google.scp.shared.clients.configclient.aws.AwsClientConfigModule.AwsCredentialSecretKey;
import javax.inject.Singleton;
import software.amazon.awssdk.auth.credentials.EnvironmentVariableCredentialsProvider;
import software.amazon.awssdk.services.sts.StsClient;

/** AWS module for privacy budget prober. */
public final class AwsPrivacyBudgetProberModule extends AbstractModule {
  private static final String COORDINATOR_A_SESSION_NAME = "coordinatorA";
  private static final String COORDINATOR_B_SESSION_NAME = "coordinatorB";

  @Provides
  @Singleton
  StsClient provideStsClient() {
    return StsClient.builder()
        .credentialsProvider(EnvironmentVariableCredentialsProvider.create())
        .build();
  }

  @Provides
  @CoordinatorACredentialsProvider
  AwsSessionCredentialsProvider provideStsAwsSessionCredentialsProviderA(
      StsClient stsClient, @CoordinatorARoleArn String coordinatorARoleArn) {
    return new StsAwsSessionCredentialsProvider(
        stsClient, coordinatorARoleArn, COORDINATOR_A_SESSION_NAME);
  }

  @Provides
  @CoordinatorBCredentialsProvider
  AwsSessionCredentialsProvider provideStsAwsSessionCredentialsProviderB(
      StsClient stsClient, @CoordinatorBRoleArn String coordinatorBRoleArn) {
    return new StsAwsSessionCredentialsProvider(
        stsClient, coordinatorBRoleArn, COORDINATOR_B_SESSION_NAME);
  }

  @Override
  protected void configure() {
    bind(String.class).annotatedWith(AwsCredentialAccessKey.class).toInstance("");
    bind(String.class).annotatedWith(AwsCredentialSecretKey.class).toInstance("");
    bind(TransactionPhaseManager.class).to(TransactionPhaseManagerImpl.class);

    install(new AwsPbsClientModule());
  }
}
