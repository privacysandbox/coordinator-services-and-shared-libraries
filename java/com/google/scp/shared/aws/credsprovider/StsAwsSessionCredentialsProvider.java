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

package com.google.scp.shared.aws.credsprovider;

import javax.inject.Singleton;
import software.amazon.awssdk.services.sts.StsClient;
import software.amazon.awssdk.services.sts.auth.StsAssumeRoleCredentialsProvider;

/**
 * Provides temporary AWS credentials using AWS Security Token Service (STS)
 *
 * <p>The credentials provided by this class are fetched directly from STS as oppposed to being
 * reused from the credentials available from e.g. the EC2 instance metadata.
 */
@Singleton
public final class StsAwsSessionCredentialsProvider extends AwsSessionCredentialsProvider {

  /** Duration for which requested credentials should be valid. */
  private static final int DURATION_SECONDS = 1000;

  /**
   * @param roleArn the role to assume and provide credentials for.
   * @param sessionName The name of the session associated with the credentials (visible in logs)
   */
  public StsAwsSessionCredentialsProvider(StsClient stsClient, String roleArn, String sessionName) {
    super(
        StsAssumeRoleCredentialsProvider.builder()
            .stsClient(stsClient)
            .refreshRequest(
                request ->
                    request
                        .durationSeconds(DURATION_SECONDS)
                        .roleArn(roleArn)
                        .roleSessionName(sessionName))
            .build(),
        () ->
            stsClient
                .assumeRole(
                    request ->
                        request
                            .durationSeconds(DURATION_SECONDS)
                            .roleArn(roleArn)
                            .roleSessionName(sessionName))
                .credentials());
  }
}
