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

package com.google.scp.shared.aws.util;

import static com.google.scp.shared.api.model.Code.INTERNAL;
import static com.google.scp.shared.mapper.TimeObjectMapper.SERIALIZATION_ERROR_RESPONSE_JSON;

import com.amazonaws.services.lambda.runtime.events.APIGatewayProxyResponseEvent;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;
import com.google.protobuf.util.JsonFormat;
import com.google.protobuf.util.JsonFormat.Printer;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Defines helper methods for use with AWS Lambda Handlers */
public final class LambdaHandlerUtil {
  private static final Logger logger = LoggerFactory.getLogger(LambdaHandlerUtil.class);

  private static final JsonFormat.Printer DEFAULT_JSON_PRINTER =
      JsonFormat.printer().includingDefaultValueFields();

  private LambdaHandlerUtil() {}

  /**
   * Sets body, statusCode, and headers for returned APIGatewayProxyResponseEvent. The proto passed
   * is serialized into JSON and set in the body of the response. If any errors occur, this returns
   * generic error and logs internal details.
   *
   * <p>NOTE: Default values of proto fields are preserved in the JSON. If the field names of proto
   * fields must also be preserved, use {@link
   * #createApiGatewayResponseFromProtoPreservingFieldNames} instead.
   */
  public static <T extends Message> APIGatewayProxyResponseEvent createApiGatewayResponseFromProto(
      T proto, int code, Map<String, String> headers) {
    return createApiGatewayResponseFromProto(DEFAULT_JSON_PRINTER, proto, code, headers);
  }

  /**
   * Sets body, statusCode, and headers for returned APIGatewayProxyResponseEvent. The proto passed
   * is serialized into JSON and set in the body of the response. If any errors occur, this returns
   * * generic error and logs internal details.
   *
   * <p>NOTE: The proto field names and the default values of proto fields are preserved in the
   * JSON.
   */
  public static <T extends Message>
      APIGatewayProxyResponseEvent createApiGatewayResponseFromProtoPreservingFieldNames(
          T proto, int code, Map<String, String> headers) {
    Printer jsonPrinter = DEFAULT_JSON_PRINTER.preservingProtoFieldNames();
    return createApiGatewayResponseFromProto(jsonPrinter, proto, code, headers);
  }

  private static <T extends Message> APIGatewayProxyResponseEvent createApiGatewayResponseFromProto(
      JsonFormat.Printer jsonPrinter, T proto, int code, Map<String, String> headers) {

    APIGatewayProxyResponseEvent response = new APIGatewayProxyResponseEvent();
    try {
      if (proto != null) {
        response.setBody(jsonPrinter.print(proto));
      }
      response.setStatusCode(code);
      if (!headers.isEmpty()) {
        response.setHeaders(headers);
      }
    } catch (InvalidProtocolBufferException e) {
      logger.error("Serialization error", e);
      response.setBody(SERIALIZATION_ERROR_RESPONSE_JSON);
      response.setStatusCode(INTERNAL.getHttpStatusCode());
    }

    return response;
  }
}
