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
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableList;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.BinaryKeysetReader;
import com.google.crypto.tink.CleartextKeysetHandle;
import com.google.crypto.tink.HybridDecrypt;
import com.google.crypto.tink.KeysetHandle;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyTypeProto.EncryptionKeyType;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.KeyDataProto.KeyData;
import com.google.scp.operator.cpio.cryptoclient.DecryptionKeyService.KeyFetchException;
import com.google.scp.operator.cpio.cryptoclient.EncryptionKeyFetchingService.EncryptionKeyFetchingServiceException;
import com.google.scp.operator.cpio.cryptoclient.model.ErrorReason;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.api.model.Code;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.testutils.crypto.MockTinkUtils;
import com.google.scp.shared.util.KeySplitUtil;
import java.util.Base64;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class MultiPartyDecryptionKeyServiceImplTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  @Mock private EncryptionKeyFetchingService coordinatorAKeyFetchingService;
  @Mock private EncryptionKeyFetchingService coordinatorBKeyFetchingService;
  @Mock private Aead aeadPrimary;
  @Mock private Aead aeadSecondary;
  @Mock private CloudAeadSelector aeadServicePrimary;
  @Mock private CloudAeadSelector aeadServiceSecondary;
  private MockTinkUtils mockTinkUtils;
  private MultiPartyDecryptionKeyServiceImpl multiPartyDecryptionKeyServiceImpl;

  @Before
  public void setup() throws Exception {
    mockTinkUtils = new MockTinkUtils();
    multiPartyDecryptionKeyServiceImpl =
        new MultiPartyDecryptionKeyServiceImpl(
            coordinatorAKeyFetchingService,
            coordinatorBKeyFetchingService,
            aeadServicePrimary,
            aeadServiceSecondary);
  }

  @Test
  public void getDecrypter_errorWithCode_NOT_FOUND() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.NOT_FOUND, "test", "test")));

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));

    assertEquals(exception.getReason(), ErrorReason.KEY_NOT_FOUND);
  }

  @Test
  public void getDecrypter_errorWithCode_PERMISSION_DENIED() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.PERMISSION_DENIED, "test", "test")));

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));

    assertEquals(exception.getReason(), ErrorReason.PERMISSION_DENIED);
  }

  @Test
  public void getDecrypter_errorWithCode_INTERNAL() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.INTERNAL, "test", "test")));

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));

    assertEquals(exception.getReason(), ErrorReason.INTERNAL);
  }

  @Test
  public void getDecrypter_errorWithCode_default() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(new EncryptionKeyFetchingServiceException(new Exception()));

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));

    assertEquals(exception.getReason(), ErrorReason.UNKNOWN_ERROR);
  }

  @Test
  public void getDecrypter_serviceNotAvailable_throwsServiceUnavailable() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.UNAVAILABLE, "test", "test")));

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));

    assertEquals(exception.getReason(), ErrorReason.KEY_SERVICE_UNAVAILABLE);
  }

  @Test
  public void getDecrypter_deadlineExceeded_throwsServiceUnavailable() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.DEADLINE_EXCEEDED, "test", "test")));

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));

    assertEquals(exception.getReason(), ErrorReason.KEY_SERVICE_UNAVAILABLE);
  }

  @Test
  public void getDecrypter_errorWithCode_UNKNOWN() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.UNKNOWN, "test", "test")));

    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));

    assertEquals(exception.getReason(), ErrorReason.UNKNOWN_ERROR);
  }

  @Test
  public void getDecrypter_getsDecrypterSingleKey() throws Exception {
    KeyData keyData =
        KeyData.newBuilder()
            .setKeyEncryptionKeyUri("abc")
            .setKeyMaterial(mockTinkUtils.getAeadKeySetJson())
            .build();
    EncryptionKey encryptionKey =
        EncryptionKey.newBuilder()
            .setName("encryptionKeys/123")
            .setEncryptionKeyType(EncryptionKeyType.SINGLE_PARTY_HYBRID_KEY)
            .setPublicKeysetHandle("12345")
            .setPublicKeyMaterial("qwert")
            .addAllKeyData(ImmutableList.of(keyData))
            .build();
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(eq("123"))).thenReturn(encryptionKey);
    when(aeadServicePrimary.getAead("abc")).thenReturn(aeadPrimary);
    when(aeadPrimary.decrypt(any(byte[].class), any(byte[].class)))
        .thenReturn(mockTinkUtils.getDecryptedKey());

    String plaintext = "test_plaintext";
    byte[] cipheredText = mockTinkUtils.getCiphertext(plaintext);
    HybridDecrypt actualHybridDecrypt = multiPartyDecryptionKeyServiceImpl.getDecrypter("123");

    assertThat(actualHybridDecrypt.decrypt(cipheredText, null)).isEqualTo(plaintext.getBytes());
    // Should only invoke fetch once.
    verify(coordinatorAKeyFetchingService, times(1)).fetchEncryptionKey(any());
  }

  @Test
  public void getDecrypter_getsDecrypterSplitKey() throws Exception {
    KeysetHandle keysetHandle =
        CleartextKeysetHandle.read(BinaryKeysetReader.withBytes(mockTinkUtils.getDecryptedKey()));
    ImmutableList<ByteString> keySplits = KeySplitUtil.xorSplit(keysetHandle, 2);
    EncryptionKey encryptionKey =
        EncryptionKey.newBuilder()
            .setName("encryptionKeys/123")
            .setEncryptionKeyType(EncryptionKeyType.MULTI_PARTY_HYBRID_EVEN_KEYSPLIT)
            .setPublicKeysetHandle("12345")
            .setPublicKeyMaterial("qwert")
            .build();
    // Each party only has a single split with the key material.
    EncryptionKey partyAKey =
        encryptionKey.toBuilder()
            .addAllKeyData(
                ImmutableList.of(
                    KeyData.newBuilder()
                        .setKeyEncryptionKeyUri("abc1")
                        .setKeyMaterial(
                            Base64.getEncoder().encodeToString("secret key1".getBytes()))
                        .build(),
                    KeyData.newBuilder().setKeyEncryptionKeyUri("abc2").build()))
            .build();
    EncryptionKey partyBKey =
        encryptionKey.toBuilder()
            .addAllKeyData(
                ImmutableList.of(
                    KeyData.newBuilder().setKeyEncryptionKeyUri("abc1").build(),
                    KeyData.newBuilder()
                        .setKeyEncryptionKeyUri("abc2")
                        .setKeyMaterial(
                            Base64.getEncoder().encodeToString("secret key2".getBytes()))
                        .build()))
            .build();
    // Set up mock key decryption to return key splits.
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(eq("123"))).thenReturn(partyAKey);
    when(coordinatorBKeyFetchingService.fetchEncryptionKey(eq("123"))).thenReturn(partyBKey);
    when(aeadServicePrimary.getAead("abc1")).thenReturn(aeadPrimary);
    when(aeadServiceSecondary.getAead("abc2")).thenReturn(aeadSecondary);
    when(aeadPrimary.decrypt(any(byte[].class), any(byte[].class)))
        .thenReturn(keySplits.get(0).toByteArray());
    when(aeadSecondary.decrypt(any(byte[].class), any(byte[].class)))
        .thenReturn(keySplits.get(1).toByteArray());

    String plaintext = "test_plaintext";
    byte[] cipheredText = mockTinkUtils.getCiphertext(plaintext);
    HybridDecrypt actualHybridDecrypt = multiPartyDecryptionKeyServiceImpl.getDecrypter("123");

    assertThat(actualHybridDecrypt.decrypt(cipheredText, null)).isEqualTo(plaintext.getBytes());
    // Verify both key splits were fetched.
    verify(coordinatorAKeyFetchingService, times(1)).fetchEncryptionKey(any());
    verify(coordinatorBKeyFetchingService, times(1)).fetchEncryptionKey(any());
  }
}
