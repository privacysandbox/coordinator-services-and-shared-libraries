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

package com.google.scp.operator.cpio.cryptoclient;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import com.google.acai.Acai;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.HybridDecrypt;
import com.google.inject.AbstractModule;
import com.google.inject.Inject;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService.KeyFetchException;
import com.google.scp.operator.cpio.cryptoclient.model.ErrorReason;
import com.google.scp.operator.cpio.cryptoclient.testing.FakePrivateKeyFetchingService;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.testutils.crypto.MockTinkUtils;
import java.util.UUID;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class DecryptionKeyServiceImplTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();
  @Rule public final Acai acai = new Acai(TestEnv.class);

  @Inject private FakePrivateKeyFetchingService keyFetchingService;
  @Mock private Aead aead;
  private MockTinkUtils mockTinkUtils;
  private DecryptionKeyServiceImpl decryptionKeyServiceImpl;

  @Before
  public void setup() throws Exception {
    mockTinkUtils = new MockTinkUtils();
    decryptionKeyServiceImpl = new DecryptionKeyServiceImpl(aead, keyFetchingService);
  }

  @Test
  public void getDecrypter_getsDecrypter() throws Exception {
    keyFetchingService.setResponse(mockTinkUtils.getAeadKeySetJson());
    when(aead.decrypt(any(byte[].class), any(byte[].class)))
        .thenReturn(mockTinkUtils.getDecryptedKey());
    when(aead.encrypt(any(byte[].class), any(byte[].class)))
        .thenReturn(mockTinkUtils.getEncryptedKey());

    String plaintext = "test_plaintext";
    byte[] cipheredText = mockTinkUtils.getCiphertext(plaintext);
    HybridDecrypt actualHybridDecrypt =
        decryptionKeyServiceImpl.getDecrypter(UUID.randomUUID().toString());
    assertThat(actualHybridDecrypt.decrypt(cipheredText, new byte[] {}))
        .isEqualTo(plaintext.getBytes());
  }

  @Test
  public void getDecrypter_errorWithCode_NOT_FOUND() throws Exception {
    keyFetchingService.setExceptionContents(Code.NOT_FOUND, /* message= */ "");

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class,
            () -> decryptionKeyServiceImpl.getDecrypter(UUID.randomUUID().toString()));

    assertEquals(exception.getReason(), ErrorReason.KEY_NOT_FOUND);
  }

  @Test
  public void getDecrypter_errorWithCode_PERMISSION_DENIED() throws Exception {
    keyFetchingService.setExceptionContents(Code.PERMISSION_DENIED, /* message= */ "");

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class,
            () -> decryptionKeyServiceImpl.getDecrypter(UUID.randomUUID().toString()));

    assertEquals(exception.getReason(), ErrorReason.PERMISSION_DENIED);
  }

  @Test
  public void getDecrypter_errorWithCode_INTERNAL() throws Exception {
    keyFetchingService.setExceptionContents(Code.INTERNAL, /* message= */ "");

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class,
            () -> decryptionKeyServiceImpl.getDecrypter(UUID.randomUUID().toString()));

    assertEquals(exception.getReason(), ErrorReason.INTERNAL);
  }

  @Test
  public void getDecrypter_errorWithCode_default() throws Exception {
    keyFetchingService.setExceptionContents(Code.UNKNOWN, /* message= */ "");

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class,
            () -> decryptionKeyServiceImpl.getDecrypter(UUID.randomUUID().toString()));

    assertEquals(exception.getReason(), ErrorReason.UNKNOWN_ERROR);
  }

  @Test
  public void getDecrypter_serviceNotAvailable_throwsServiceUnavailable() throws Exception {
    keyFetchingService.setExceptionContents(Code.UNAVAILABLE, "Service Not Available");

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class,
            () -> decryptionKeyServiceImpl.getDecrypter(UUID.randomUUID().toString()));

    assertEquals(exception.getReason(), ErrorReason.KEY_SERVICE_UNAVAILABLE);
  }

  @Test
  public void getDecrypter_serviceTimesOut_throwsServiceUnavailable() throws Exception {
    keyFetchingService.setExceptionContents(Code.DEADLINE_EXCEEDED, "Deadline Exceeded");

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class,
            () -> decryptionKeyServiceImpl.getDecrypter(UUID.randomUUID().toString()));

    assertEquals(exception.getReason(), ErrorReason.KEY_SERVICE_UNAVAILABLE);
  }

  @Test
  public void getDecrypter_errorWithCode_UNKNOWN() throws Exception {
    keyFetchingService.setResponse("bad test string");

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class,
            () -> decryptionKeyServiceImpl.getDecrypter(UUID.randomUUID().toString()));

    assertEquals(exception.getReason(), ErrorReason.UNKNOWN_ERROR);
  }

  private static final class TestEnv extends AbstractModule {

    @Override
    protected void configure() {}
  }
}
