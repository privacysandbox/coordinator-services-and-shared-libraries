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

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.common.cache.CacheBuilder;
import com.google.common.cache.CacheLoader;
import com.google.common.cache.LoadingCache;
import com.google.common.collect.ImmutableList;
import com.google.common.util.concurrent.UncheckedExecutionException;
import com.google.crypto.tink.HybridDecrypt;
import com.google.crypto.tink.JsonKeysetReader;
import com.google.crypto.tink.KeysetHandle;
import com.google.inject.BindingAnnotation;
import com.google.inject.Inject;
import com.google.protobuf.ByteString;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.EncryptionKeyProto.EncryptionKey;
import com.google.scp.coordinator.protos.keymanagement.shared.api.v1.KeyDataProto.KeyData;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorAAead;
import com.google.scp.operator.cpio.cryptoclient.Annotations.CoordinatorBAead;
import com.google.scp.operator.cpio.cryptoclient.EncryptionKeyFetchingService.EncryptionKeyFetchingServiceException;
import com.google.scp.operator.cpio.cryptoclient.model.ErrorReason;
import com.google.scp.shared.api.exception.ServiceException;
import com.google.scp.shared.crypto.tink.CloudAeadSelector;
import com.google.scp.shared.util.KeySplitUtil;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;
import java.security.GeneralSecurityException;
import java.util.Base64;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * Implementation for retrieving and decrypting keys from the KMS. This version uses the encryption
 * key API which also supports multi-party keys.
 */
public final class MultiPartyDecryptionKeyServiceImpl implements DecryptionKeyService {

  private static final int MAX_CACHE_SIZE = 100;
  private static final long CACHE_ENTRY_TTL_SEC = 3600;
  private static final int CONCURRENCY_LEVEL = Runtime.getRuntime().availableProcessors();
  private final CloudAeadSelector coordinatorAAeadService;
  private final CloudAeadSelector coordinatorBAeadService;
  private final EncryptionKeyFetchingService coordinatorAEncryptionKeyFetchingService;
  private final EncryptionKeyFetchingService coordinatorBEncryptionKeyFetchingService;
  private final LoadingCache<String, HybridDecrypt> decypterCache =
      CacheBuilder.newBuilder()
          .maximumSize(MAX_CACHE_SIZE)
          .expireAfterWrite(CACHE_ENTRY_TTL_SEC, TimeUnit.SECONDS)
          .concurrencyLevel(CONCURRENCY_LEVEL)
          .build(
              new CacheLoader<String, HybridDecrypt>() {
                @Override
                public HybridDecrypt load(final String keyId) throws KeyFetchException {
                  return createDecrypter(keyId);
                }
              });

  /** Creates a new instance of the {@code MultiPartyDecryptionKeyServiceImpl} class. */
  @Inject
  public MultiPartyDecryptionKeyServiceImpl(
      @CoordinatorAEncryptionKeyFetchingService
          EncryptionKeyFetchingService coordinatorAEncryptionKeyFetchingService,
      @CoordinatorBEncryptionKeyFetchingService
          EncryptionKeyFetchingService coordinatorBEncryptionKeyFetchingService,
      @CoordinatorAAead CloudAeadSelector coordinatorAAeadService,
      @CoordinatorBAead CloudAeadSelector coordinatorBAeadService) {
    this.coordinatorAEncryptionKeyFetchingService = coordinatorAEncryptionKeyFetchingService;
    this.coordinatorBEncryptionKeyFetchingService = coordinatorBEncryptionKeyFetchingService;
    this.coordinatorAAeadService = coordinatorAAeadService;
    this.coordinatorBAeadService = coordinatorBAeadService;
  }

  /** Returns the decrypter for the provided key. */
  @Override
  public HybridDecrypt getDecrypter(String keyId) throws KeyFetchException {
    try {
      return decypterCache.get(keyId);
    } catch (ExecutionException | UncheckedExecutionException e) {
      ErrorReason reason = ErrorReason.UNKNOWN_ERROR;
      if (e.getCause() instanceof KeyFetchException) {
        reason = ((KeyFetchException) e.getCause()).getReason();
      }
      throw new KeyFetchException("Failed to get key with id: " + keyId, reason, e);
    }
  }

  /** Key fetching service for coordinator A. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorAEncryptionKeyFetchingService {}

  /** Key fetching service for coordinator B. */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface CoordinatorBEncryptionKeyFetchingService {}

  private HybridDecrypt createDecrypter(String keyId) throws KeyFetchException {
    try {
      var primaryEncryptionKey = coordinatorAEncryptionKeyFetchingService.fetchEncryptionKey(keyId);

      switch (primaryEncryptionKey.getEncryptionKeyType()) {
        case SINGLE_PARTY_HYBRID_KEY:
          return createDecrypterSingleKey(primaryEncryptionKey);
        case MULTI_PARTY_HYBRID_EVEN_KEYSPLIT:
          var secondaryEncryptionKey =
              coordinatorBEncryptionKeyFetchingService.fetchEncryptionKey(keyId);
          return createDecrypterSplitKey(primaryEncryptionKey, secondaryEncryptionKey);
        default:
          throw new KeyFetchException(
              "Unsupported encryption key type.", ErrorReason.UNKNOWN_ERROR);
      }

    } catch (EncryptionKeyFetchingServiceException e) {
      throw generateKeyFetchException(e);
    } catch (GeneralSecurityException e) {
      throw new KeyFetchException(e, ErrorReason.KEY_DECRYPTION_ERROR);
    } catch (IOException e) {
      throw new KeyFetchException("Failed to fetch key ID: " + keyId, ErrorReason.UNKNOWN_ERROR, e);
    }
  }

  private HybridDecrypt createDecrypterSingleKey(EncryptionKey encryptionKey)
      throws GeneralSecurityException, IOException {
    var encryptionKeyData = getOwnerKeyData(encryptionKey);
    var aead = coordinatorAAeadService.getAead(encryptionKeyData.getKeyEncryptionKeyUri());
    var keysetHandle =
        KeysetHandle.read(JsonKeysetReader.withString(encryptionKeyData.getKeyMaterial()), aead);
    return keysetHandle.getPrimitive(HybridDecrypt.class);
  }

  private HybridDecrypt createDecrypterSplitKey(
      EncryptionKey encryptionKeyA, EncryptionKey encryptionKeyB)
      throws GeneralSecurityException, IOException {
    // Split A.
    var encryptionKeyAData = getOwnerKeyData(encryptionKeyA);
    var aeadA = coordinatorAAeadService.getAead(encryptionKeyAData.getKeyEncryptionKeyUri());
    var splitA =
        aeadA.decrypt(Base64.getDecoder().decode(encryptionKeyAData.getKeyMaterial()), new byte[0]);

    // Split B.
    var encryptionKeyBData = getOwnerKeyData(encryptionKeyB);
    var aeadB = coordinatorBAeadService.getAead(encryptionKeyBData.getKeyEncryptionKeyUri());
    var splitB =
        aeadB.decrypt(Base64.getDecoder().decode(encryptionKeyBData.getKeyMaterial()), new byte[0]);

    // Reconstruct.
    var keySetHandle =
        KeySplitUtil.reconstructXorKeysetHandle(
            ImmutableList.of(ByteString.copyFrom(splitA), ByteString.copyFrom(splitB)));
    return keySetHandle.getPrimitive(HybridDecrypt.class);
  }

  /** Find {@code KeyData} object owned by the coordinator. */
  private KeyData getOwnerKeyData(EncryptionKey encryptionKey) {
    // Should only be one key data with key material per coordinator.
    return encryptionKey.getKeyDataList().stream()
        .filter(keyData -> !keyData.getKeyMaterial().isEmpty())
        .findFirst()
        .get();
  }

  private static KeyFetchException generateKeyFetchException(
      EncryptionKeyFetchingServiceException e) {
    if (e.getCause() instanceof ServiceException) {
      switch (((ServiceException) e.getCause()).getErrorCode()) {
        case NOT_FOUND:
          return new KeyFetchException(e, ErrorReason.KEY_NOT_FOUND);
        case PERMISSION_DENIED:
        case UNAUTHENTICATED:
          return new KeyFetchException(e, ErrorReason.PERMISSION_DENIED);
        case INTERNAL:
          return new KeyFetchException(e, ErrorReason.INTERNAL);
        case UNAVAILABLE:
        case DEADLINE_EXCEEDED:
          return new KeyFetchException(e, ErrorReason.KEY_SERVICE_UNAVAILABLE);
        default:
          return new KeyFetchException(e, ErrorReason.UNKNOWN_ERROR);
      }
    }
    return new KeyFetchException(e, ErrorReason.UNKNOWN_ERROR);
  }
}
