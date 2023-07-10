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

package com.google.scp.shared.crypto.tink.kmstoolenclave;

import com.google.common.collect.ImmutableList;
import com.google.common.primitives.Bytes;
import com.google.crypto.tink.Aead;
import com.google.scp.shared.aws.credsprovider.AwsSessionCredentialsProvider;
import java.io.IOException;
import java.lang.ProcessBuilder.Redirect;
import java.security.GeneralSecurityException;
import java.util.Base64;
import java.util.Collections;
import java.util.List;
import software.amazon.awssdk.auth.credentials.AwsSessionCredentials;
import software.amazon.awssdk.regions.Region;

/**
 * Tink Aead implementation using the AWS KMS Tool Enclave. Only supports decryption and does not
 * support associatedData. Only works inside a Nitro Enclave.
 *
 * <p>This class is a thin wrapper around the CLI tool and all cryptographic operations are handled
 * by the subprocess spawned by this class.
 *
 * <ul>
 *   <li><a
 *       href="https://github.com/aws/aws-nitro-enclaves-sdk-c/tree/main/bin/kmstool-enclave-cli">KMS
 *       Enclave CLI Docs</a>
 *   <li><a href="https://github.com/aws/aws-nitro-enclaves-sdk-c">Nitro SDK Docs</a>
 * </ul>
 */
public final class KmsToolEnclaveAead implements Aead {

  private static String CLI_PATH = "/kmstool_enclave_cli";
  private final Region region;
  private final AwsSessionCredentialsProvider credentialsProvider;

  /**
   * @param region The AWS region the enclave CLI should sent requests to.
   */
  public KmsToolEnclaveAead(AwsSessionCredentialsProvider credentialsProvider, Region region) {
    this.region = region;
    this.credentialsProvider = credentialsProvider;
  }

  @Override
  public byte[] encrypt(byte[] plaintext, byte[] associatedData) {
    throw new UnsupportedOperationException("kmstool_enclave_cli does not support encryption");
  }

  @Override
  public byte[] decrypt(byte[] ciphertext, byte[] associatedData) throws GeneralSecurityException {
    if (associatedData != null && associatedData.length != 0) {
      throw new IllegalArgumentException(
          "kmstool_enclave_cli does not support associatedData:"
              + " https://github.com/aws/aws-nitro-enclaves-sdk-c/issues/35");
    }

    AwsSessionCredentials credentials = credentialsProvider.resolveCredentials();

    // Note: input (ciphertext param) and output (stdout) for kmstool are base64 encoded.
    ImmutableList<String> command =
        new ImmutableList.Builder<String>()
            .add(CLI_PATH)
            .add("decrypt")
            .add("--region", region.toString())
            .add("--aws-access-key-id", credentials.accessKeyId())
            .add("--aws-secret-access-key", credentials.secretAccessKey())
            .add("--aws-session-token", credentials.sessionToken())
            .add("--ciphertext", new String(Base64.getEncoder().encode(ciphertext)))
            .build();

    ProcessBuilder pb = new ProcessBuilder(command);
    // TODO: Set up proper debug logging. STDERR logging is better than nothing though.
    pb.redirectError(Redirect.INHERIT);

    try {
      Process process = pb.start();
      // TODO: Should we add a timeout to the process in case it siezes?
      int exitCode = process.waitFor();
      if (exitCode != 0) {
        throw new GeneralSecurityException("Non-zero exit code from kmstool cli");
      }
      // The kmstool_enclave_cli output format: 'PLAINTEXT: <decrypted_content>\n'
      List<Byte> kmsToolOutput = Bytes.asList(process.getInputStream().readAllBytes());
      List<Byte> outputPrefix = Bytes.asList("PLAINTEXT: ".getBytes());
      if (!kmsToolOutput.isEmpty()
          && Collections.indexOfSubList(kmsToolOutput, outputPrefix) == 0) {
        kmsToolOutput = kmsToolOutput.subList(outputPrefix.size(), kmsToolOutput.size());
      }
      if (!kmsToolOutput.isEmpty() && kmsToolOutput.get(kmsToolOutput.size() - 1) == '\n') {
        kmsToolOutput = kmsToolOutput.subList(0, kmsToolOutput.size() - 1);
      }
      return Base64.getDecoder().decode(Bytes.toArray(kmsToolOutput));
    } catch (IllegalArgumentException e) {
      // Base64.decode() will throw if not provided valid base64.
      throw new GeneralSecurityException("Failed to handle process output", e);
    } catch (IOException e) {
      throw new GeneralSecurityException("Failed to start process", e);
    } catch (InterruptedException e) {
      throw new GeneralSecurityException("Process interrupted", e);
    }
  }
}
