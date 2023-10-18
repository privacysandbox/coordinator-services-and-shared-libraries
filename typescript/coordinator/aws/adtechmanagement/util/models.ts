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

/**
 * The metadata of an Adtech.
 */
export interface Adtech {
  originURL: string;
  adtechID: string;
  awsID: string;
  iamRoleArn: string;
  iamRoleName: string;
}

/**
 * The structure of the request body when making an API request to /onboardAdtech endpoint.
 */
export interface OnboardingRequest {
  originURL: string;
  adtechID: string;
  awsID: string;
}

/**
 * The structure of the request body when making an API request to /offboardAdtech endpoint.
 */
export interface OffboardingRequest extends Adtech {}

/**
 * The ReadRequest interface represents the structure of the request body when making an API request to /readAdtech endpoint.
 */
export interface ReadRequest {
  iamRoleArn?: string;
}
/**
 * The event object passed from the API Gateway to the Lambdas
 */
export interface HttpApiEvent {
  version: string;
  routeKey: string;
  rawPath: string;
  rawQueryString: string;
  cookies: string[];
  headers: { [key: string]: string };
  queryStringParameters: { [key: string]: string };
  requestContext: {
    accountId: string;
    apiId: string;
    authorizer: {
      jwt: {
        claims: { [key: string]: string };
        scopes: string[];
      };
    };
    domainName: string;
    domainPrefix: string;
    http: {
      method: string;
      path: string;
      protocol: string;
      sourceIp: string;
      userAgent: string;
    };
    requestId: string;
    routeKey: string;
    stage: string;
    time: string;
    timeEpoch: number;
  };
  body: string;
  pathParameters: { [key: string]: string };
  isBase64Encoded: boolean;
  stageVariables: { [key: string]: string };
}
