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

package com.google.scp.operator.shared.testing;

import com.google.scp.operator.shared.injection.modules.BaseAwsClientsModule;
import com.google.scp.shared.testutils.aws.AwsHermeticTestHelper;
import org.testcontainers.containers.localstack.LocalStackContainer;
import org.testcontainers.containers.localstack.LocalStackContainer.Service;
import software.amazon.awssdk.auth.credentials.AwsBasicCredentials;
import software.amazon.awssdk.auth.credentials.StaticCredentialsProvider;
import software.amazon.awssdk.awscore.client.builder.AwsClientBuilder;
import software.amazon.awssdk.awscore.client.builder.AwsSyncClientBuilder;
import software.amazon.awssdk.http.apache.ApacheHttpClient;

/**
 * The AwsClientsModule implementation to be used with the AwsHermeticTestHelper that allows
 * subclasses to specify the LocalStackContainer instantiated by the AwsHermeticTestHelper.
 */
public abstract class HermeticAwsClientsModule extends BaseAwsClientsModule {

  /** Gets the LocalStackContainer created for the test to bind the endpoint override urls. */
  protected abstract LocalStackContainer getLocalStack();

  @Override
  protected <
          ClientType,
          BuilderType extends
              AwsSyncClientBuilder<BuilderType, ClientType>
                  & AwsClientBuilder<BuilderType, ClientType>>
      ClientType initializeClient(BuilderType builder) {
    var localStackContainer = getLocalStack();
    builder
        .httpClient(ApacheHttpClient.builder().build())
        .region(AwsHermeticTestHelper.getRegion())
        .credentialsProvider(
            StaticCredentialsProvider.create(
                AwsBasicCredentials.create(
                    localStackContainer.getAccessKey(), localStackContainer.getSecretKey())))
        // Since all override endpoints for each service are at the same url, any Service type can
        // be used
        .endpointOverride(getLocalStack().getEndpointOverride(Service.LAMBDA));
    return builder.build();
  }
}
