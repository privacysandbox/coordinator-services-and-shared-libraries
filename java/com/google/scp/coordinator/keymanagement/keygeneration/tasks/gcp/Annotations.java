package com.google.scp.coordinator.keymanagement.keygeneration.tasks.gcp;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/** Annotations for GCP Key Generation Tasks. */
public final class Annotations {

  private Annotations() {}

  /**
   * Binds instance of Kms Key Aead used to encrypt private keys split sent to Coordinator B. Not to
   * be confused with {@link
   * com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKeyAead}
   * which is used to encrypt DataKeys from Coordinator B to Coordinator A.
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PeerCoordinatorKmsKeyAead {}

  /**
   * Binds the (unparsed String) URI of the key used for encrypting the private keys splits sent to
   * Coordinator B.
   *
   * <p>See {@link
   * com.google.scp.coordinator.protos.keymanagement.shared.backend.EncryptionKeyProto.EncryptionKey#getKeyEncryptionKeyUri}
   * for more information.
   *
   * <p>Not to * be confused with {@link *
   * com.google.scp.coordinator.keymanagement.keystorage.tasks.common.Annotations.CoordinatorKekUri}
   * which is used to encrypt DataKeys from Coordinator B to Coordinator A.
   */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  public @interface PeerCoordinatorKeyEncryptionKeyUri {}
}
