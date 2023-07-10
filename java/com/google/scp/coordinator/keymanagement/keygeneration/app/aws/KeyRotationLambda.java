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
import com.google.crypto.tink.CleartextKeysetHandle;
import com.google.crypto.tink.JsonKeysetWriter;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.crypto.tink.integration.awskmsv2.AwsKmsV2Client;
import com.google.scp.shared.util.KeyParams;
import com.google.scp.shared.util.PublicKeyConversionUtil;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.URI;
import java.security.GeneralSecurityException;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;
import software.amazon.awssdk.auth.credentials.AwsCredentialsProvider;
import software.amazon.awssdk.auth.credentials.EnvironmentVariableCredentialsProvider;
import software.amazon.awssdk.http.SdkHttpClient;
import software.amazon.awssdk.http.urlconnection.UrlConnectionHttpClient;
import software.amazon.awssdk.regions.Region;
import software.amazon.awssdk.services.dynamodb.DynamoDbClient;
import software.amazon.awssdk.services.dynamodb.DynamoDbClientBuilder;
import software.amazon.awssdk.services.dynamodb.model.AttributeValue;
import software.amazon.awssdk.services.dynamodb.model.PutItemRequest;

/**
 * AWS Lambda function for generating asymmetric keys and storing them in DynamoDB.
 *
 * <p>Each time the handler is called: a new key is generated, the new key is encrypted using an AWS
 * KMS symmetric key, and the encrypted key is stored in DynamoDB.
 *
 * @deprecated use {@link KeyGenerationLambda} instead, this entrypoint exists for migration and
 *     load test purposes.
 */
@Deprecated
public final class KeyRotationLambda implements RequestHandler<ScheduledEvent, String> {

  private static final String tableEnvName = "KEYSTORE_TABLE_NAME";
  private static final String tableRegionName = "KEYSTORE_TABLE_REGION";
  private static final String encryptionKeyEnvName = "ENCRYPTION_KEY_ARN";
  private static final String endpointOverrideName = "KEYSTORE_ENDPOINT_OVERRIDE";
  private final AwsKmsV2Client kmsClient;
  private final DynamoDbClient ddbClient;
  /** Environment variable map. */
  private final Map<String, String> env;

  /** Creates handler with correct defaults for lambda deployment. */
  public KeyRotationLambda() throws Exception {
    this.env = System.getenv();

    // TODO: investigate the usage of a logging library instead.
    System.out.println("Building HTTP Client");
    var httpClient = UrlConnectionHttpClient.builder().build();
    System.out.println("Building Credentials Provider");
    var credentialsProvider = EnvironmentVariableCredentialsProvider.create();
    System.out.println("Building Tink AWS Client");
    this.kmsClient = buildAwsV2Client(httpClient, credentialsProvider);
    System.out.println("Building DDB Client");
    this.ddbClient = buildDdbClient(httpClient, credentialsProvider);
  }

  /** Creates handler with custom clients for testing purposes. */
  public KeyRotationLambda(
      AwsKmsV2Client kmsClient, DynamoDbClient ddbClient, Map<String, String> env) {
    this.kmsClient = kmsClient;
    this.ddbClient = ddbClient;
    this.env = env;
  }

  /** Returns a string version of the KeysetHandle with the key unencrypted. */
  private static String toJsonCleartext(KeysetHandle keysetHandle) {
    ByteArrayOutputStream keyStream = new ByteArrayOutputStream();
    try {
      CleartextKeysetHandle.write(keysetHandle, JsonKeysetWriter.withOutputStream(keyStream));
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
    return keyStream.toString();
  }

  /** Returns a string version of the KeysetHandle with the key encrypted using AWS KMS. */
  private String toJsonCiphertext(KeysetHandle keysetHandle) {
    ByteArrayOutputStream keyStream = new ByteArrayOutputStream();
    try {
      keysetHandle.write(
          JsonKeysetWriter.withOutputStream(keyStream), kmsClient.getAead(getMasterKeyUri()));
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    } catch (GeneralSecurityException e) {
      throw new AssertionError("Failed to encrypt keysetHandle", e);
    }
    return keyStream.toString();
  }

  private static AwsKmsV2Client buildAwsV2Client(
      SdkHttpClient httpClient, AwsCredentialsProvider credentialsProvider) {
    try {
      return new AwsKmsV2Client()
          .withHttpClient(httpClient)
          .withCredentialsProvider(credentialsProvider);
    } catch (GeneralSecurityException e) {
      throw new AssertionError("Failed to build AwsKmsV2Client", e);
    }
  }

  private DynamoDbClient buildDdbClient(
      SdkHttpClient httpClient, AwsCredentialsProvider credentialsProvider) throws Exception {
    String endpoint = env.getOrDefault(endpointOverrideName, "");
    DynamoDbClientBuilder builder =
        DynamoDbClient.builder().credentialsProvider(credentialsProvider).httpClient(httpClient);
    try {
      String region = env.get(tableRegionName);
      builder.region(Region.of(region));
    } catch (Exception e) {
      throw new Exception(
          "Table region not found in environment. Please set "
              + tableRegionName
              + ".\nCurrent env: "
              + env);
    }
    if (endpoint.isEmpty()) {
      return builder.build();
    }
    return builder.endpointOverride(URI.create(endpoint)).build();
  }

  /** Returns the configured DynamoDB table name. */
  public String getTableName() {
    return env.getOrDefault(tableEnvName, "unspecified_table");
  }

  /** Returns the configured URI of the AWS encryption key. */
  public String getMasterKeyUri() {
    String keyArn = env.getOrDefault(encryptionKeyEnvName, "unspecified_key_id");

    return AwsKmsV2Client.PREFIX + keyArn;
  }

  /** Encrypts the provided key and stores it in DynamoDB. */
  private void uploadKey(KeysetHandle privateKeyHandle) throws GeneralSecurityException {
    String uuid = UUID.randomUUID().toString();

    KeysetHandle publicKeyHandle = privateKeyHandle.getPublicKeysetHandle();
    String tableName = getTableName();
    Instant now = Instant.now();
    Instant expiration = now.plus(7, ChronoUnit.DAYS);

    // Use a hashMap because this is a short-lived variable and adding a dependency on guava would
    // impact function cold start performance.
    HashMap<String, AttributeValue> itemValues = new HashMap<>();
    itemValues.put("KeyId", AttributeValue.builder().s(uuid).build());
    itemValues.put(
        "PublicKey", AttributeValue.builder().s(toJsonCleartext(publicKeyHandle)).build());
    itemValues.put(
        "PrivateKey", AttributeValue.builder().s(toJsonCiphertext(privateKeyHandle)).build());
    itemValues.put(
        "CreationTime", AttributeValue.builder().n(Long.toString(now.toEpochMilli())).build());
    itemValues.put(
        "ExpirationTime",
        AttributeValue.builder().n(Long.toString(expiration.toEpochMilli())).build());
    itemValues.put(
        "PublicKeyMaterial",
        AttributeValue.builder().s(PublicKeyConversionUtil.getPublicKey(publicKeyHandle)).build());

    System.out.println("building request");
    PutItemRequest request = PutItemRequest.builder().tableName(tableName).item(itemValues).build();

    System.out.println("uploading item: " + uuid);
    ddbClient.putItem(request);
    System.out.println("item uploaded");
  }

  @Override
  public String handleRequest(ScheduledEvent event, Context context) {
    try {
      // TODO: Is there a performance penalty for calling this once per request?
      HybridConfig.register();

      System.out.println("Config complete!");
      KeysetHandle privateKeysetHandle =
          KeysetHandle.generateNew(KeyParams.getDefaultKeyTemplate());
      System.out.println("Private key initialized!");
      KeysetHandle publicKeysetHandle = privateKeysetHandle.getPublicKeysetHandle();
      System.out.println("public key initialized!");

      System.out.println("Public Key:\n" + toJsonCleartext(publicKeysetHandle));
      System.out.println("Private Key:\n" + toJsonCiphertext(privateKeysetHandle));

      uploadKey(privateKeysetHandle);

      System.out.println("Key uploaded!");
    } catch (GeneralSecurityException e) {
      System.err.println("Key setup failed");
      e.printStackTrace(System.err);
      return "Key setup error.";
    }

    return "Success!";
  }
}
