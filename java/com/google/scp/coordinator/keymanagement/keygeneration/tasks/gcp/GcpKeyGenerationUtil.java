package com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp;

import com.google.auth.oauth2.GoogleCredentials;
import com.google.common.annotations.Beta;
import com.google.crypto.tink.Aead;
import com.google.crypto.tink.integration.gcpkms.GcpKmsClient;
import com.google.protobuf.ByteString;
import com.google.scp.shared.util.KeysetHandleSerializerUtil;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Base64;
import java.util.Optional;

/** Defines helper methods to GCP Create Key tasks and modules. */
public final class GcpKeyGenerationUtil {

  /** Helper method to get Aead from URI with an option to use credentials. */
  public static Aead getAead(String kmsKeyUri, Optional<GoogleCredentials> credentials) {
    GcpKmsClient client = new GcpKmsClient();
    try {
      if (credentials.isPresent()) {
        client.withCredentials(credentials.get());
      } else {
        client.withDefaultCredentials();
      }
      return client.getAead(kmsKeyUri);
    } catch (GeneralSecurityException e) {
      throw new RuntimeException(
          String.format("Error getting gcloud Aead with uri %s.", kmsKeyUri), e);
    }
  }

  /** Returns an Aead from the encoded KeysetHandle string. */
  @Beta
  public static Aead getAeadFromEncodedKeysetHandle(String encodedKeysetHandle) {
    ByteString keysetHandleByteString =
        ByteString.copyFrom(Base64.getDecoder().decode(encodedKeysetHandle));
    try {
      return KeysetHandleSerializerUtil.fromBinaryCleartext(keysetHandleByteString)
          .getPrimitive(Aead.class);
    } catch (GeneralSecurityException | IOException e) {
      throw new RuntimeException(e);
    }
  }
}
