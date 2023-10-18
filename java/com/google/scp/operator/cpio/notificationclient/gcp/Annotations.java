package com.google.scp.operator.cpio.notificationclient.gcp;

import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.ElementType.PARAMETER;
import static java.lang.annotation.RetentionPolicy.RUNTIME;

import com.google.inject.BindingAnnotation;
import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/** Annotations to be used for the GCP Notification Client. */
final class Annotations {

  /** Annotation for the Pub/Sub publisher client */
  @BindingAnnotation
  @Target({FIELD, PARAMETER, METHOD})
  @Retention(RUNTIME)
  @interface GcpNotificationClientPublisher {}

}
