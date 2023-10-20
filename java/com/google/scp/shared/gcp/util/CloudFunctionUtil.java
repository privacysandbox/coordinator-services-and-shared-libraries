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

package com.google.scp.shared.gcp.util;

import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.mapper.TimeObjectMapper.SERIALIZATION_ERROR_RESPONSE_JSON;

import com.google.cloud.functions.HttpResponse;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;
import com.google.protobuf.util.JsonFormat;
import com.google.protobuf.util.JsonFormat.Printer;
import java.io.BufferedWriter;
import java.io.IOException;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Defines helper methods for use with GCP Cloud Functions */
public final class CloudFunctionUtil {
  private static final Logger logger = LoggerFactory.getLogger(CloudFunctionUtil.class);

  private static final JsonFormat.Printer DEFAULT_JSON_PRINTER =
      JsonFormat.printer().includingDefaultValueFields();

  private CloudFunctionUtil() {}

  /**
   * Sets body, statusCode, and headers for HttpResponse from a proto message. If there is a
   * InvalidProtocolBufferException, returns generic error and logs internal details
   */
  public static <T extends Message> void createCloudFunctionResponseFromProto(
      HttpResponse response, T bodyObject, int statusCode, Map<String, String> headers)
      throws IOException {
    createCloudFunctionResponseFromProto(
        DEFAULT_JSON_PRINTER, response, bodyObject, statusCode, headers);
  }

  /**
   * Sets body, statusCode, and headers for HttpResponse from a proto message. If there is a
   * InvalidProtocolBufferException, returns generic error and logs internal details
   */
  public static <T extends Message> void createCloudFunctionResponseFromProtoPreservingFieldNames(
      HttpResponse response, T bodyObject, int statusCode, Map<String, String> headers)
      throws IOException {
    Printer jsonPrinter = DEFAULT_JSON_PRINTER.preservingProtoFieldNames();
    createCloudFunctionResponseFromProto(jsonPrinter, response, bodyObject, statusCode, headers);
  }

  private static <T extends Message> void createCloudFunctionResponseFromProto(
      JsonFormat.Printer jsonPrinter,
      HttpResponse response,
      T bodyObject,
      int statusCode,
      Map<String, String> headers)
      throws IOException {
    BufferedWriter writer = response.getWriter();

    try {
      writer.write(jsonPrinter.print(bodyObject));
      response.setStatusCode(statusCode);
      headers.forEach(response::appendHeader);
    } catch (InvalidProtocolBufferException e) {
      logger.error("Serialization error", e);
      writer.write(SERIALIZATION_ERROR_RESPONSE_JSON);
      response.setStatusCode(INTERNAL.getHttpStatusCode());
    }
  }
}
