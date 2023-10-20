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

import com.beust.jcommander.Parameter;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.SplitKeyDecryptionClientSelector;
import com.google.scp.coordinator.keymanagement.keygeneration.app.common.SplitKeyParameterClientSelector;
import java.net.URI;
import java.util.Optional;
import software.amazon.awssdk.regions.Region;

/** Runtime arguments for split-key Key Generation Application */
public final class AwsSplitKeyGenerationArgs {

  @Parameter(
      names = "--param_client",
      description = "Parameter client implementation: ARGS or AWS (for SSM Client)")
  private SplitKeyParameterClientSelector paramClient = SplitKeyParameterClientSelector.ARGS;

  @Parameter(
      names = "--decryption_client",
      description =
          "What type of client to use for decrypting incoming DataKeys to use for encryption.")
  private SplitKeyDecryptionClientSelector decryptionClient =
      SplitKeyDecryptionClientSelector.NON_ENCLAVE;

  @Parameter(
      names = "--key_generation_queue_url",
      description = "The URL of the AWS SQS Queue for Key Generation messages")
  private String keyGenerationSqsUrl = "";

  @Parameter(
      names = "--key_generation_queue_max_wait_time_seconds",
      description =
          "Key Generation SQS max wait time on message receipt, in seconds (maximum 20 seconds)")
  private int keyGenerationSqsMaxWaitTimeSeconds;

  @Parameter(
      names = "--key_generation_queue_message_lease_seconds",
      description =
          "The value for the Key Generation SQS message lease timeout (maximum 600 seconds)")
  private int keyGenerationQueueMessageLeaseSeconds;

  @Parameter(
      names = "--application_region_override",
      description = "Overrides the region of the compute instance.")
  private String applicationRegionOverride = "";

  @Parameter(
      names = "--ec2_endpoint_override",
      description = "Optional EC2 service endpoint override URI")
  private String ec2EndpointOverride = "";

  @Parameter(names = "--sqs_endpoint_override", description = "Optional Sqs Endpoint override URI")
  private String sqsEndpointOverride = "";

  @Parameter(names = "--ssm_endpoint_override", description = "Optional Ssm Endpoint override URI")
  private String ssmEndpointOverride = "";

  @Parameter(names = "--kms_endpoint_override", description = "Optional KMS Endpoint override URI")
  private String kmsEndpointOverride = "";

  @Parameter(names = "--sts_endpoint_override", description = "Optional STS Endpoint override URI")
  private String stsEndpointOverride = "";

  @Parameter(
      names = "--access_key",
      description =
          "Optional access key for AWS credentials. If this parameter (and --secret_key) is set,"
              + " the static credentials provider will be used.")
  private String accessKey = "";

  @Parameter(
      names = "--secret_key",
      description =
          "Optional secret key for AWS credentials.  If this parameter (and --access_key) is set,"
              + " the static credentials provider will be used.")
  private String secretKey = "";

  @Parameter(
      names = "--coordinator_b_region",
      description = "Which region to authenticate requests against when talking to Coordinator B.")
  private String coordinatorBRegion = "us-east-1";

  @Parameter(
      names = "--coordinator_b_assume_role_arn",
      description =
          "Which role to assume for making requests to other coordinator services. If empty, the"
              + " default credentials are used instead.")
  private String coordinatorBAssumeRoleArn = "";

  @Parameter(
      names = "--aws_metadata_endpoint_override",
      description =
          "Optional ec2 metadata endpoint override URI. This is used to get EC2 metadata"
              + " information including tags, and profile credentials. If this parameter is set,"
              + " the instance credentials provider will be used.")
  private String awsMetadataEndpointOverride = "";

  @Parameter(
      names = "--key_storage_service_base_url",
      description =
          "Base URL for Coordinator B HTTP services (used for key exchange service and key storage"
              + " service)")
  private String keyStorageServiceBaseUrl = "";

  @Parameter(
      names = "--get_data_key_override_base_url",
      description =
          "If provided, uses this base url instead of key_storage_service_base_url for fetching"
              + " data keys -- for use in tests")
  private String getDataKeyOverrideBaseUrl = "";

  @Parameter(names = "--key_db_name", description = "Table name of KeyDb")
  private String keyDbName = "";

  @Parameter(names = "--key_db_region", description = "Region of KeyDb")
  private String keyDbRegion = "";

  @Parameter(
      names = "--key_db_endpoint_override",
      description = "Endpoint override for KeyDb if necessary")
  private String keyDbEndpointOverride = "";

  @Parameter(
      names = "--kms_key_uri",
      description = "KMS Key URI used to encrypt the private key splits")
  private String kmsKeyUri = "";

  @Parameter(
      names = "--signature_key_id",
      description =
          "KMS Key ARN used by this coordinator to sign public key material. If not present, does"
              + " not sign.")
  private String signatureKeyId = "";

  @Parameter(
      names = "--signature_algorithm",
      description = "AWS KMS Key Signature Algorithm used by signature_key_id")
  private String signatureAlgorithm = "EDCSA_SHA_256";

  @Parameter(
      names = "--key_count",
      description = "Generate keys until we have this number of valid keys in the database")
  private int keyCount = 5;

  @Parameter(
      names = "--validity_in_days",
      description = "Number of days after creation to mark keys as expired")
  private int validityInDays = 8;

  @Parameter(
      names = "--ttl_in_days",
      description = "Number of days after creation before purging a key from the database")
  private int ttlInDays = 365;

  @Parameter(names = "--key_id_type", description = "Key ID Type")
  private String keyIdType = "";

  SplitKeyParameterClientSelector getParamClient() {
    return paramClient;
  }

  public SplitKeyDecryptionClientSelector getDecryptionClient() {
    return decryptionClient;
  }

  public String getKeyGenerationSqsUrl() {
    return keyGenerationSqsUrl;
  }

  public int getKeyGenerationSqsMaxWaitTimeSeconds() {
    return keyGenerationSqsMaxWaitTimeSeconds;
  }

  public int getKeyGenerationQueueMessageLeaseSeconds() {
    return keyGenerationQueueMessageLeaseSeconds;
  }

  public Optional<String> getKeyIdType() {
    return Optional.of(keyIdType).filter(type -> !type.isEmpty());
  }

  public String getApplicationRegionOverride() {
    return applicationRegionOverride;
  }

  public URI getEc2EndpointOverride() {
    return URI.create(ec2EndpointOverride);
  }

  public URI getSqsEndpointOverride() {
    return URI.create(sqsEndpointOverride);
  }

  public URI getSsmEndpointOverride() {
    return URI.create(ssmEndpointOverride);
  }

  public URI getKmsEndpointOverride() {
    return URI.create(kmsEndpointOverride);
  }

  public Optional<URI> getStsEndpointOverride() {
    return Optional.of(kmsEndpointOverride)
        .filter(endpoint -> !endpoint.isEmpty())
        .map(URI::create);
  }

  public String getAccessKey() {
    return accessKey;
  }

  public String getSecretKey() {
    return secretKey;
  }

  public Region getCoordinatorBRegion() {
    return Region.of(coordinatorBRegion);
  }

  public Optional<String> getCoordinatorBAssumeRoleArn() {
    return Optional.of(coordinatorBAssumeRoleArn).filter(arn -> !arn.isEmpty());
  }

  public String getAwsMetadataEndpointOverride() {
    return awsMetadataEndpointOverride;
  }

  public String getKeyStorageServiceBaseUrl() {
    return keyStorageServiceBaseUrl;
  }

  public Optional<String> getGetDataKeyOverrideBaseUrl() {
    return Optional.of(getDataKeyOverrideBaseUrl).filter(url -> !url.isEmpty());
  }

  public String getKeyDbName() {
    return keyDbName;
  }

  public String getKeyDbRegion() {
    return keyDbRegion;
  }

  public String getKeyDbEndpointOverride() {
    return keyDbEndpointOverride;
  }

  public String getKmsKeyUri() {
    return kmsKeyUri;
  }

  public Optional<String> getSignatureKeyId() {
    return Optional.of(signatureKeyId).filter(keyId -> !keyId.isEmpty());
  }

  public String getSignatureAlgorithm() {
    return signatureAlgorithm;
  }

  public int getKeyCount() {
    return keyCount;
  }

  public int getValidityInDays() {
    return validityInDays;
  }

  public int getTtlInDays() {
    return ttlInDays;
  }
}
