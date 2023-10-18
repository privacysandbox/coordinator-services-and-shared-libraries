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
import { handler } from "../src/adtech_offboarding_lambda";
import * as DatabaseFunctions from "../util/db_operations";
import * as IAMFunctions from "../util/role_operations";
import { HttpApiEvent, OffboardingRequest } from "../util/models";
import { INTERNAL_SERVER_ERROR_STATUS_CODE } from "../util/global_vars";
import {
  attachRolePolicy,
  createIAMRole,
  deleteIAMRole,
  detachRolePolicy,
} from "../util/role_operations";
import {
  addToAdtechMgmtDB,
  deleteFromAdtechMgmtDB,
  deleteFromPBSAuthDB,
} from "../util/db_operations";

describe("handler", () => {
  const mockAdtechOffboardingRequest: OffboardingRequest = {
    originURL: "https://example.com",
    adtechID: "123456",
    awsID: "1234567890",
    iamRoleArn: "arn:aws:iam::1234567890:role/adtech-123456",
    iamRoleName: "adtech-123456",
  };

  const mockHttpOffboardingApiEvent: HttpApiEvent = {
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
    body: JSON.stringify(mockAdtechOffboardingRequest),
    pathParameters: {
      name: "Bard",
    },
    isBase64Encoded: false,
    stageVariables: {
      foo: "bar",
    },
  };

  beforeEach(() => {
    spyOn(DatabaseFunctions, "addToAdtechMgmtDB");
    spyOn(DatabaseFunctions, "addToPBSAuthDB");
    spyOn(IAMFunctions, "createIAMRole");
    spyOn(IAMFunctions, "attachRolePolicy");
  });

  it("should detach iam role policy from IAM role", async () => {
    spyOn(IAMFunctions, "detachRolePolicy");
    await handler(mockHttpOffboardingApiEvent);
    expect(IAMFunctions.detachRolePolicy).toHaveBeenCalledWith("adtech-123456");
  });

  it("should delete the IAM role", async () => {
    spyOn(IAMFunctions, "detachRolePolicy");
    spyOn(IAMFunctions, "deleteIAMRole");
    await handler(mockHttpOffboardingApiEvent);
    expect(IAMFunctions.deleteIAMRole).toHaveBeenCalledWith("adtech-123456");
  });

  it("should throw error when policy doesn't exist", async () => {
    spyOn(IAMFunctions, "detachRolePolicy").and.throwError(
      "Policy doesn't exist.",
    );
    const response = await handler(mockHttpOffboardingApiEvent);
    expect(response).toEqual({
      statusCode: INTERNAL_SERVER_ERROR_STATUS_CODE,
      body: JSON.stringify({
        message: `Adtech 123456 offboarding failed`,
      }),
    });
  });

  it("should undo policy detachment when offboarding process fails at deleting IAM role", async () => {
    spyOn(IAMFunctions, "detachRolePolicy");
    spyOn(IAMFunctions, "deleteIAMRole").and.throwError(
      "Failed to delete IAM role.",
    );
    await handler(mockHttpOffboardingApiEvent);
    expect(attachRolePolicy).toHaveBeenCalledWith("adtech-123456");
  });

  it("should undo delete AM role action when onboarding process fails to delete metadata from adtech mgmt db", async () => {
    spyOn(IAMFunctions, "detachRolePolicy");
    spyOn(IAMFunctions, "deleteIAMRole");
    spyOn(DatabaseFunctions, "deleteFromAdtechMgmtDB").and.throwError(
      "Failed to delete Adtech from Adtech Mgmt DB.",
    );
    await handler(mockHttpOffboardingApiEvent);
    expect(createIAMRole).toHaveBeenCalledWith("1234567890");
  });

  it("should undo delete from adtech mgmt db action when onboarding process fails to delete origin mapping to pbs auth db", async () => {
    spyOn(IAMFunctions, "detachRolePolicy");
    spyOn(IAMFunctions, "deleteIAMRole");
    spyOn(DatabaseFunctions, "deleteFromAdtechMgmtDB");
    spyOn(DatabaseFunctions, "deleteFromPBSAuthDB").and.throwError(
      "Failed to add origin mapping to PBS Auth DB",
    );

    await handler(mockHttpOffboardingApiEvent);
    expect(addToAdtechMgmtDB).toHaveBeenCalledWith(
      mockAdtechOffboardingRequest,
    );
  });
});
