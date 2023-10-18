//  Copyright 2023 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

import { Adtech, HttpApiEvent, ReadRequest } from "../util/models";
import * as DatabaseFunctions from "../util/db_operations";
import { handler } from "../src/adtech_read_lambda";
import { converter } from "../util/db_operations";
import { NativeAttributeValue } from "@aws-sdk/util-dynamodb";
import {
  INTERNAL_SERVER_ERROR_STATUS_CODE,
  READ_ALL_ADTECHS_ERROR_MESSAGE,
  REQUEST_SUCCESS_STATUS_CODE,
} from "../util/global_vars";
describe("handler", () => {
  const createMockReadAdtechApiEvent = (
    requestBody: ReadRequest,
  ): HttpApiEvent => {
    return {
      version: "2020-05-26",
      routeKey: "GET /hello",
      rawPath: "/hello",
      rawQueryString: "name=Bard",
      cookies: ["cookie1", "cookie2"],
      headers: {
        Host: "example.com",
        Accept: "application/json",
      },
      queryStringParameters: {
        name: "Bard",
      },
      requestContext: {
        accountId: "123456789012",
        apiId: "123456789012",
        authorizer: {
          jwt: {
            claims: {
              sub: "123456789012",
            },
            scopes: ["scope1", "scope2"],
          },
        },
        domainName: "example.com",
        domainPrefix: "api",
        http: {
          method: "GET",
          path: "/hello",
          protocol: "HTTP/1.1",
          sourceIp: "192.168.1.1",
          userAgent:
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.84 Safari/537.36",
        },
        requestId: "12345678-9012-3456-7890-123456789012",
        routeKey: "GET /hello",
        stage: "dev",
        time: "2023-07-17T15:41:30.000Z",
        timeEpoch: 1657854130000,
      },
      body: JSON.stringify(requestBody),
      pathParameters: {
        name: "Bard",
      },
      isBase64Encoded: false,
      stageVariables: {
        foo: "bar",
      },
    };
  };
  const mockDynamoDBRecord: Record<string, NativeAttributeValue> = {
    iam_role: "a-role-arn",
    iam_role_name: "a-role-name",
    origin_url: "an-origin-url",
    adtech_id: "an-adtech-id",
    aws_id: "an-aws-id",
  };
  it("should call readFromAdtechMgmtDB if iamRoleArn exists in request body", async () => {
    spyOn(DatabaseFunctions, "readFromAdtechMgmtDB");
    await handler(
      createMockReadAdtechApiEvent({
        iamRoleArn: "arn:aws:iam::1234567890:role/adtech-123456",
      }),
    );
    expect(DatabaseFunctions.readFromAdtechMgmtDB).toHaveBeenCalledWith(
      "arn:aws:iam::1234567890:role/adtech-123456",
    );
  });
  it("should call readAllFromAdtechMgmtDB if no iamRoleArn exists in request body", async () => {
    spyOn(DatabaseFunctions, "readAllFromAdtechMgmtDB");
    await handler(createMockReadAdtechApiEvent({}));
    expect(DatabaseFunctions.readAllFromAdtechMgmtDB).toHaveBeenCalledWith();
  });
  it("should convert DynamoDB record to Adtech object", () => {
    expect(converter(mockDynamoDBRecord)).toEqual({
      iamRoleArn: "a-role-arn",
      iamRoleName: "a-role-name",
      originURL: "an-origin-url",
      adtechID: "an-adtech-id",
      awsID: "an-aws-id",
    });
  });
  it("should return success code if read single adtech request succeeded", async () => {
    const mockReceivedAdtech: Adtech = {
      iamRoleArn: "a-role-arn",
      iamRoleName: "a-role-name",
      originURL: "an-origin-url",
      adtechID: "an-adtech-id",
      awsID: "an-aws-id",
    };
    spyOn(DatabaseFunctions, "readFromAdtechMgmtDB").and.returnValue(
      Promise.resolve(mockDynamoDBRecord),
    );
    const response = await handler(
      createMockReadAdtechApiEvent({ iamRoleArn: "a-role-arn" }),
    );
    expect(response).toEqual({
      statusCode: REQUEST_SUCCESS_STATUS_CODE,
      body: JSON.stringify([mockReceivedAdtech]),
    });
  });
  it("should return error code if read all adtechs request fails", async () => {
    const mockReceivedAdtech: Adtech = {
      iamRoleArn: "a-role-arn",
      iamRoleName: "a-role-name",
      originURL: "an-origin-url",
      adtechID: "an-adtech-id",
      awsID: "an-aws-id",
    };
    spyOn(DatabaseFunctions, "readAllFromAdtechMgmtDB").and.throwError(
      READ_ALL_ADTECHS_ERROR_MESSAGE,
    );
    const response = await handler(createMockReadAdtechApiEvent({}));
    expect(response).toEqual({
      statusCode: INTERNAL_SERVER_ERROR_STATUS_CODE,
      message: READ_ALL_ADTECHS_ERROR_MESSAGE,
    });
  });
});
