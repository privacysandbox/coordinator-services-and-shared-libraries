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

package com.google.scp.coordinator.keymanagement.keystorage.service.gcp;

import com.google.crypto.tink.Aead;
import com.google.inject.AbstractModule;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionAead;
import com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.EncryptionKeyUri;
import com.google.scp.coordinator.keymanagement.shared.util.GcpAeadProvider;
import java.util.Map;

class AwsEncryptionAeadModule extends AbstractModule {

  private static final String AWS_KMS_URI_ENV_VAR = "AWS_KMS_URI";
  private static final String AWS_KMS_ROLE_ARN_ENV_VAR = "AWS_KMS_ROLE_ARN";
  Map<String, String> env;

  AwsEncryptionAeadModule(Map<String, String> env) {
    this.env = env;
  }

  @Override
  protected void configure() {
    String awsKmsUri = env.get(AWS_KMS_URI_ENV_VAR);
    String awsKmsRoleArn = env.get(AWS_KMS_ROLE_ARN_ENV_VAR);
    bind(Aead.class)
        .annotatedWith(EncryptionAead.class)
        .toInstance(GcpAeadProvider.getAwsAead(awsKmsUri, awsKmsRoleArn));
    bind(String.class).annotatedWith(EncryptionKeyUri.class).toInstance(awsKmsUri);
  }
}
