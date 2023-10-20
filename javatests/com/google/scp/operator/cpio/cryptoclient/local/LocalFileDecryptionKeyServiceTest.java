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

package com.google.scp.operator.cpio.cryptoclient.local;

import static com.google.common.truth.Truth.assertThat;

import com.google.acai.Acai;
import com.google.common.jimfs.Configuration;
import com.google.common.jimfs.Jimfs;
import com.google.crypto.tink.HybridEncrypt;
import com.google.crypto.tink.KeysetHandle;
import com.google.crypto.tink.hybrid.HybridConfig;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.cpio.cryptoclient.local.LocalFileDecryptionKeyServiceModule.DecryptionKeyFilePath;
import java.io.IOException;
import java.nio.file.FileSystem;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.GeneralSecurityException;
import java.util.UUID;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class LocalFileDecryptionKeyServiceTest {

  private static Path decryptionKeyPath;
  private static KeysetHandle key;

  @Rule public final Acai acai = new Acai(TestEnv.class);

  // Under test
  @Inject LocalFileDecryptionKeyService localFileDecryptionKeyService;

  private static Path setUpWithFile() {
    try {
      FileSystem filesystem =
          Jimfs.newFileSystem(Configuration.unix().toBuilder().setWorkingDirectory("/").build());
      Path workingDirectory = filesystem.getPath("decryption");
      Files.createDirectories(workingDirectory);
      decryptionKeyPath = workingDirectory.resolve("key.pb");

      HybridConfig.register();
      HybridKeyFileGenerator.generateKeysetHandle(decryptionKeyPath);
      key = HybridKeyFileGenerator.readKeysetHandle(decryptionKeyPath);
    } catch (IOException | GeneralSecurityException e) {
      throw new RuntimeException(e);
    }
    return decryptionKeyPath;
  }

  private static byte[] generateCiphertext(byte[] plaintext) throws GeneralSecurityException {
    var hybridEncrypt = key.getPublicKeysetHandle().getPrimitive(HybridEncrypt.class);
    return hybridEncrypt.encrypt(plaintext, null);
  }

  @Test
  public void getKey() throws Exception {
    String decryptionKeyId = UUID.randomUUID().toString();
    var chosenPlaintext = "chosen plaintext".getBytes();
    var ciphertext = generateCiphertext(chosenPlaintext);

    var key = localFileDecryptionKeyService.getDecrypter(decryptionKeyId);

    assertThat(key.decrypt(ciphertext, null)).isEqualTo(chosenPlaintext);
  }

  private static final class TestEnv extends AbstractModule {

    @Override
    protected void configure() {
      bind(Path.class).annotatedWith(DecryptionKeyFilePath.class).toInstance(setUpWithFile());
    }
  }
}
