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

import "jasmine";
import { handler } from "../src/adtech_onboarding_lambda";
import * as DatabaseFunctions from "../util/db_operations";
import * as IAMFunctions from "../util/role_operations";
import { HttpApiEvent, OnboardingRequest } from "../util/models";
import { Role } from "@aws-sdk/client-iam";
import {
  INTERNAL_SERVER_ERROR_STATUS_CODE,
  MALFORMED_POLICY_CUSTOM_ERROR_MESSAGE,
} from "../util/global_vars";
import { deleteIAMRole, detachRolePolicy } from "../util/role_operations";
import {
  deleteFromAdtechMgmtDB,
  deleteFromPBSAuthDB,
} from "../util/db_operations";

describe("handler", () => {
  const mockAdtechOnboardingRequest: OnboardingRequest = {
    awsID: "1234567890",
    adtechID: "123456",
    originURL: "https://example.com",
  };
  const mockHttpOnboardingApiEvent: HttpApiEvent = {
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
    body: JSON.stringify(mockAdtechOnboardingRequest),
    pathParameters: {
      name: "Bard",
    },
    isBase64Encoded: false,
    stageVariables: {
      foo: "bar",
    },
  };

  const mockIAMRole: Role = {
    Path: "/my-path/to/role",
    RoleName: "my-role-name",
    RoleId: "1234567890123456",
    Arn: "arn:aws:iam::123456789012:role/my-role-name",
    CreateDate: new Date("2023-07-21T13:00:00Z"),
    AssumeRolePolicyDocument: `{
    "Version": "2012-10-17",
    "Statement": [
      {
        "Effect": "Allow",
        "Principal": {
          "AWS": "*"
        },
        "Action": "sts:AssumeRole"
      }
    ]
  }`,
    Description: "This is my role description.",
    MaxSessionDuration: 3600,
    Tags: [
      {
        Key: "Name",
        Value: "My Role",
      },
      {
        Key: "Environment",
        Value: "Production",
      },
    ],
  };

  beforeEach(() => {
    spyOn(DatabaseFunctions, "deleteFromAdtechMgmtDB");
    spyOn(DatabaseFunctions, "deleteFromPBSAuthDB");
    spyOn(IAMFunctions, "deleteIAMRole");
    spyOn(IAMFunctions, "detachRolePolicy");
  });

  it("should create an IAM role", async () => {
    spyOn(IAMFunctions, "createIAMRole").and.returnValue(
      Promise.resolve(mockIAMRole),
    );
    await handler(mockHttpOnboardingApiEvent);
    expect(IAMFunctions.createIAMRole).toHaveBeenCalledWith("1234567890");
  });

  it("should attach role policy to IAM role", async () => {
    spyOn(IAMFunctions, "createIAMRole").and.returnValue(
      Promise.resolve(mockIAMRole),
    );
    spyOn(IAMFunctions, "attachRolePolicy");
    await handler(mockHttpOnboardingApiEvent);
    expect(IAMFunctions.attachRolePolicy).toHaveBeenCalledWith("my-role-name");
  });

  it("should throw error and provide correct message when invalid aws id is entered", async () => {
    const fakeError = new Error();
    fakeError.name = "MalformedPolicyDocumentException";
    spyOn(IAMFunctions, "createIAMRole").and.throwError(fakeError);
    const response = await handler(mockHttpOnboardingApiEvent);
    expect(response).toEqual({
      statusCode: INTERNAL_SERVER_ERROR_STATUS_CODE,
      body: JSON.stringify({
        message: MALFORMED_POLICY_CUSTOM_ERROR_MESSAGE,
      }),
    });
  });

  it("should throw error and provide correct message when role already exists", async () => {
    const fakeError = new Error();
    fakeError.name = "EntityAlreadyExistsException";
    spyOn(IAMFunctions, "createIAMRole").and.throwError(fakeError);
    const response = await handler(mockHttpOnboardingApiEvent);
    expect(response).toEqual({
      statusCode: INTERNAL_SERVER_ERROR_STATUS_CODE,
      body: JSON.stringify({
        message: "An Adtech with AWS ID 1234567890 is already onboarded.",
      }),
    });
  });

  it("should undo create IAM role action when onboarding process fails at role attachment", async () => {
    spyOn(IAMFunctions, "createIAMRole").and.returnValue(
      Promise.resolve(mockIAMRole),
    );
    spyOn(IAMFunctions, "attachRolePolicy").and.throwError(
      "Policy does not exist.",
    );
    await handler(mockHttpOnboardingApiEvent);
    expect(deleteIAMRole).toHaveBeenCalledWith("my-role-name");
  });

  it("should policy attachment action when onboarding process fails to add metadata to adtech mgmt db", async () => {
    spyOn(IAMFunctions, "createIAMRole").and.returnValue(
      Promise.resolve(mockIAMRole),
    );
    spyOn(IAMFunctions, "attachRolePolicy");
    spyOn(DatabaseFunctions, "addToAdtechMgmtDB").and.throwError(
      "Failed to add Adtech to Adtech Mgmt DB.",
    );
    await handler(mockHttpOnboardingApiEvent);
    expect(detachRolePolicy).toHaveBeenCalledWith("my-role-name");
  });

  it("should undo add to adtech mgmt db action  when onboarding process fails to add origin mapping to pbs auth db", async () => {
    spyOn(IAMFunctions, "createIAMRole").and.returnValue(
      Promise.resolve(mockIAMRole),
    );
    spyOn(IAMFunctions, "attachRolePolicy");
    spyOn(DatabaseFunctions, "addToAdtechMgmtDB");
    spyOn(DatabaseFunctions, "addToPBSAuthDB").and.throwError(
      "Failed to add origin mapping to PBS Auth DB",
    );

    await handler(mockHttpOnboardingApiEvent);
    expect(deleteFromAdtechMgmtDB).toHaveBeenCalledWith(
      "arn:aws:iam::123456789012:role/my-role-name",
    );
  });
});
