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

package com.google.scp.shared.api.model;

/**
 * Enum Representation of google.rpc.Code:
 * https:github.com/googleapis/api-common-protos/blob/master/google/rpc/code.proto
 */
public enum Code {
  /**
   * Not an error, returned on success
   *
   * <p>HTTP Mapping: 200 OK
   */
  OK(0, 200),

  /**
   * The operation has been accepted for processing, but may not have started yet.
   *
   * <p>HTTP Mapping: 202 Accepted
   */
  ACCEPTED(17, 202),

  /**
   * The operation was cancelled, typically by the caller.
   *
   * <p>HTTP Mapping: 499 Client Closed Request
   */
  CANCELLED(1, 499),

  /**
   * Unknown error. For example, this error may be returned when a `Status` value received from
   * another address space belongs to an error space that is not known in this address space. Also
   * errors raised by APIs that do not return enough error information may be converted to this
   * error.
   *
   * <p>HTTP Mapping: 500 Internal Server Error
   */
  UNKNOWN(2, 500),

  /**
   * The client specified an invalid argument. Note that this differs from `FAILED_PRECONDITION`.
   * `INVALID_ARGUMENT` indicates arguments that are problematic regardless of the state of the
   * system (e.g., a malformed file name).
   *
   * <p>HTTP Mapping: 400 Bad Request
   */
  INVALID_ARGUMENT(3, 400),

  /**
   * The deadline expired before the operation could complete. For operations that change the state
   * of the system, this error may be returned even if the operation has completed successfully. For
   * example, a successful response from a server could have been delayed long enough for the
   * deadline to expire.
   *
   * <p>HTTP Mapping: 504 Gateway Timeout
   */
  DEADLINE_EXCEEDED(4, 504),

  /**
   * Some requested entity (e.g., file or directory) was not found.
   *
   * <p>Note to server developers: if a request is denied for an entire class of users, such as
   * gradual feature rollout or undocumented allowlist, `NOT_FOUND` may be used. If a request is
   * denied for some users within a class of users, such as user-based access control,
   * `PERMISSION_DENIED` must be used.
   *
   * <p>HTTP Mapping: 404 Not Found
   */
  NOT_FOUND(5, 404),

  /**
   * The entity that a client attempted to create (e.g., file or directory) already exists.
   *
   * <p>HTTP Mapping: 409 Conflict ALREADY_EXISTS(6, 409),
   */
  ALREADY_EXISTS(6, 409),

  /**
   * /** The caller does not have permission to execute the specified operation. `PERMISSION_DENIED`
   * must not be used for rejections caused by exhausting some resource (use `RESOURCE_EXHAUSTED`
   * instead for those errors). `PERMISSION_DENIED` must not be used if the caller can not be
   * identified (use `UNAUTHENTICATED` instead for those errors). This error code does not imply the
   * request is valid or the requested entity exists or satisfies other pre-conditions.
   *
   * <p>HTTP Mapping: 403 Forbidden
   */
  PERMISSION_DENIED(7, 403),

  /**
   * The request does not have valid authentication credentials for the operation.
   *
   * <p>HTTP Mapping: 401 Unauthorized
   */
  UNAUTHENTICATED(16, 401),

  /**
   * Some resource has been exhausted, perhaps a per-user quota, or perhaps the entire file system
   * is out of space.
   *
   * <p>HTTP Mapping: 429 Too Many Requests
   */
  RESOURCE_EXHAUSTED(8, 429),

  /**
   * The operation was rejected because the system is not in a state required for the operation's
   * execution. For example, the directory to be deleted is non-empty, an rmdir operation is applied
   * to a non-directory, etc.
   *
   * <p>Service implementors can use the following guidelines to decide between
   * `FAILED_PRECONDITION`, `ABORTED`, and `UNAVAILABLE`: (a) Use `UNAVAILABLE` if the client can
   * retry just the failing call. (b) Use `ABORTED` if the client should retry at a higher level
   * (e.g., when a client-specified test-and-set fails, indicating the client should restart a
   * read-modify-write sequence). (c) Use `FAILED_PRECONDITION` if the client should not retry until
   * the system state has been explicitly fixed. E.g., if an "rmdir" fails because the directory is
   * non-empty, `FAILED_PRECONDITION` should be returned since the client should not retry unless
   * the files are deleted from the directory.
   *
   * <p>HTTP Mapping: 400 Bad Request
   */
  FAILED_PRECONDITION(9, 400),

  /**
   * The operation was aborted, typically due to a concurrency issue such as a sequencer check
   * failure or transaction abort.
   *
   * <p>See the guidelines above for deciding between `FAILED_PRECONDITION`, `ABORTED`, and
   * `UNAVAILABLE`.
   *
   * <p>HTTP Mapping: 409 Conflict
   */
  ABORTED(10, 409),

  /**
   * The operation was attempted past the valid range. E.g., seeking or reading past end-of-file.
   *
   * <p>Unlike `INVALID_ARGUMENT`, this error indicates a problem that may be fixed if the system
   * state changes. For example, a 32-bit file system will generate `INVALID_ARGUMENT` if asked to
   * read at an offset that is not in the range [0,2^32-1], but it will generate `OUT_OF_RANGE` if
   * asked to read from an offset past the current file size.
   *
   * <p>There is a fair bit of overlap between `FAILED_PRECONDITION` and `OUT_OF_RANGE`. We
   * recommend using `OUT_OF_RANGE` (the more specific error) when it applies so that callers who
   * are iterating through a space can easily look for an `OUT_OF_RANGE` error to detect when they
   * are done.
   *
   * <p>HTTP Mapping: 400 Bad Request
   */
  OUT_OF_RANGE(11, 400),

  /**
   * The operation is not implemented or is not supported/enabled in this service.
   *
   * <p>HTTP Mapping: 501 Not Implemented
   */
  UNIMPLEMENTED(12, 501),

  /**
   * Internal errors. This means that some invariants expected by the underlying system have been
   * broken. This error code is reserved for serious errors.
   *
   * <p>HTTP Mapping: 500 Internal Server Error
   */
  INTERNAL(13, 500),

  /**
   * The service is currently unavailable. This is most likely a transient condition, which can be
   * corrected by retrying with a backoff. Note that it is not always safe to retry non-idempotent
   * operations.
   *
   * <p>See the guidelines above for deciding between `FAILED_PRECONDITION`, `ABORTED`, and
   * `UNAVAILABLE`.
   *
   * <p>HTTP Mapping: 503 Service Unavailable
   */
  UNAVAILABLE(14, 503),

  /**
   * Unrecoverable data loss or corruption.
   *
   * <p>HTTP Mapping: 500 Internal Server Error
   */
  DATA_LOSS(15, 500);

  private final int rpcStatusCode;
  private final int httpStatusCode;

  Code(int rpcStatusCode, int httpStatusCode) {
    this.rpcStatusCode = rpcStatusCode;
    this.httpStatusCode = httpStatusCode;
  }

  public static Code fromRpcStatusCode(int internalCode) {
    for (Code code : Code.values()) {
      if (code.getRpcStatusCode() == internalCode) {
        return code;
      }
    }
    throw new IllegalArgumentException("Unknown RPC status code: " + internalCode);
  }

  public int getRpcStatusCode() {
    return rpcStatusCode;
  }

  public int getHttpStatusCode() {
    return httpStatusCode;
  }
}
