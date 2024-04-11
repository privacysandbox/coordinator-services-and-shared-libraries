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
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atMost;
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
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
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
  private static final int EXCEPTION_CACHE_THRESHOLD = 3;

  private long decrypterCacheEntryTtlSec = 28800;
  private long exceptionCacheEntryTtlSec = 10;

  @Before
  public void setup() throws Exception {
    mockTinkUtils = new MockTinkUtils();
    multiPartyDecryptionKeyServiceImpl =
        new MultiPartyDecryptionKeyServiceImpl(
            coordinatorAKeyFetchingService,
            coordinatorBKeyFetchingService,
            aeadServicePrimary,
            aeadServiceSecondary,
            decrypterCacheEntryTtlSec,
            exceptionCacheEntryTtlSec);
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
  public void getDecrypter_returnsCachedKeys_afterSuccessfulKeyFetch() throws Exception {
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
    String plaintext = "test_plaintext";
    byte[] cipheredText = mockTinkUtils.getCiphertext(plaintext);

    // Throws 2 exception then -> a key then -> exception again.
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(eq("123")))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.NOT_FOUND, "test", "test")))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.NOT_FOUND, "test", "test")))
        .thenReturn(partyAKey)
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.NOT_FOUND, "test", "test")));
    when(coordinatorBKeyFetchingService.fetchEncryptionKey(eq("123"))).thenReturn(partyBKey);
    when(aeadServicePrimary.getAead("abc1")).thenReturn(aeadPrimary);
    when(aeadServiceSecondary.getAead("abc2")).thenReturn(aeadSecondary);
    when(aeadPrimary.decrypt(any(byte[].class), any(byte[].class)))
        .thenReturn(keySplits.get(0).toByteArray());
    when(aeadSecondary.decrypt(any(byte[].class), any(byte[].class)))
        .thenReturn(keySplits.get(1).toByteArray());

    // Verify exception thrown the first two times and successful key fetches from the third
    // attempt.
    for (int i = 0; i < 5; i++) {
      if (i < 2) {
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));
      } else {
        HybridDecrypt actualHybridDecrypt = multiPartyDecryptionKeyServiceImpl.getDecrypter("123");
        assertThat(actualHybridDecrypt.decrypt(cipheredText, null)).isEqualTo(plaintext.getBytes());
      }
    }
    // Ensure Key Fetching service isn't called after a successful fetch for a keyId.
    verify(coordinatorAKeyFetchingService, times(3)).fetchEncryptionKey("123");
  }

  @Test
  public void getDecrypter_returnsCachedExceptions_afterThreshold() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.NOT_FOUND, "test", "test")));
    for (int i = 0; i < 5; i++) {
      assertThrows(
          KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));
    }
    verify(coordinatorAKeyFetchingService, times(EXCEPTION_CACHE_THRESHOLD))
        .fetchEncryptionKey("123");
  }

  @Test
  public void getDecrypter_multiThreads_returnsCachedExceptions() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.NOT_FOUND, "test", "test")));
    ExecutorService executor = Executors.newFixedThreadPool(5);

    for (int i = 0; i < 50; i++) {
      executor.execute(
          () ->
              assertThrows(
                  KeyFetchException.class,
                  () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123")));
    }
    executor.shutdown();

    assertTrue(executor.awaitTermination(60, TimeUnit.SECONDS));
    // To prevent flakiness from concurrent fetch requests, the limit is set to twice the threshold.
    verify(coordinatorAKeyFetchingService, atMost(EXCEPTION_CACHE_THRESHOLD * 2))
        .fetchEncryptionKey("123");
  }

  @Test
  public void getDecrypter_transientExceptions_notCached() throws Exception {
    when(coordinatorAKeyFetchingService.fetchEncryptionKey(anyString()))
        .thenThrow(
            new EncryptionKeyFetchingServiceException(
                new ServiceException(Code.DEADLINE_EXCEEDED, "test", "test")));
    for (int i = 0; i < 5; i++) {
      assertThrows(
          KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));
    }
    verify(coordinatorAKeyFetchingService, times(5)).fetchEncryptionKey("123");
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
  public void getDecrypter_getsDecrypterNoEncryptionKeyType() throws Exception {
    KeyData keyData =
        KeyData.newBuilder()
            .setKeyEncryptionKeyUri("abc")
            .setKeyMaterial(mockTinkUtils.getAeadKeySetJson())
            .build();
    EncryptionKey encryptionKey =
        EncryptionKey.newBuilder()
            .setName("encryptionKeys/123")
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
    KeyFetchException exception =
        assertThrows(
            KeyFetchException.class, () -> multiPartyDecryptionKeyServiceImpl.getDecrypter("123"));

    assertEquals(exception.getReason(), ErrorReason.UNSUPPORTED_ENCRYPTION_KEY_TYPE_ERROR);
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
