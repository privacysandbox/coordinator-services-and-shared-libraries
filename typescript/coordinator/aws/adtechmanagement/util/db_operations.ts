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

import {
  DeleteItemCommandOutput,
  DynamoDBClient,
  GetItemCommandOutput,
  PutItemCommandOutput,
  ScanCommandOutput,
} from "@aws-sdk/client-dynamodb";
import {
  DeleteCommand,
  DynamoDBDocumentClient,
  GetCommand,
  PutCommand,
  ScanCommand,
} from "@aws-sdk/lib-dynamodb";
import { NativeAttributeValue } from "@aws-sdk/util-dynamodb";
import { Adtech } from "./models";

const dynamoClient = new DynamoDBClient({});
const dynamoDocClient = DynamoDBDocumentClient.from(dynamoClient, {
  marshallOptions: {
    removeUndefinedValues: true,
  },
});
/**
 * Adds adtech metadata as an entry to the Adtech Management database
 * @param {Adtech} adtech - An Adtech's metadata
 * @returns {PutItemCommandOutput}
 */
export const addToAdtechMgmtDB = async (
  adtech: Adtech,
): Promise<PutItemCommandOutput> => {
  const command = new PutCommand({
    TableName: process.env.ADTECH_MGMT_DB_TABLE_NAME,
    Item: {
      iam_role: adtech.iamRoleArn,
      iam_role_name: adtech.iamRoleName,
      origin_url: adtech.originURL,
      adtech_id: adtech.adtechID,
      aws_id: adtech.awsID,
    },
  });
  return dynamoDocClient.send(command);
};
/**
 * Deletes adtech metadata from the Adtech Management database
 * @param {string} iamRoleArn of Adtech to delete
 * @returns {DeleteItemCommandOutput}
 */
export const deleteFromAdtechMgmtDB = async (
  iamRoleArn: string,
): Promise<DeleteItemCommandOutput> => {
  const command = new DeleteCommand({
    TableName: process.env.ADTECH_MGMT_DB_TABLE_NAME,
    Key: {
      iam_role: iamRoleArn,
    },
  });
  return dynamoDocClient.send(command);
};
/**
 * Reads a single adtech from the Adtech Management database
 * @param {string} iamRoleArn of Adtech to read
 * @returns {Record<string, NativeAttributeValue>}
 */
export const readFromAdtechMgmtDB = async (
  iamRoleArn: string,
): Promise<Record<string, NativeAttributeValue>> => {
  const command = new GetCommand({
    TableName: process.env.ADTECH_MGMT_DB_TABLE_NAME,
    Key: {
      iam_role: iamRoleArn,
    },
  });
  return (await dynamoDocClient.send(command)).Item!;
};
/**
 * Reads all adtech from the Adtech Management database
 * @returns {Record<string, NativeAttributeValue>[]}
 */
export const readAllFromAdtechMgmtDB = async (): Promise<
  Record<string, NativeAttributeValue>[]
> => {
  const command = new ScanCommand({
    TableName: process.env.ADTECH_MGMT_DB_TABLE_NAME,
  });
  return (await dynamoDocClient.send(command)).Items!;
};
/**
 * Adds origin mapping (mapping from iam role to origin URL) to the PBS Auth database
 * @param {string} iamRoleArn - Adtech's IAM role ARN
 * @param {string} originURL - origin URL for an Adtech
 * @returns {PutItemCommandOutput} Response
 */
export const addToPBSAuthDB = async (
  iamRoleArn: string,
  originURL: string,
): Promise<PutItemCommandOutput> => {
  const command = new PutCommand({
    TableName: process.env.PBS_AUTH_DB_TABLE_NAME,
    Item: {
      iam_role: iamRoleArn,
      origin: originURL,
    },
  });
  return dynamoDocClient.send(command);
};
/**
 * Deletes origin mapping from the PBS Auth database
 * @param {string} iamRoleArn - Adtech's IAM role ARN
 * @returns {DeleteItemCommandOutput}
 */
export const deleteFromPBSAuthDB = async (
  iamRoleArn: string,
): Promise<DeleteItemCommandOutput> => {
  const command = new DeleteCommand({
    TableName: process.env.PBS_AUTH_DB_TABLE_NAME,
    Key: {
      iam_role: iamRoleArn,
    },
  });
  return dynamoDocClient.send(command);
};
/**
 * Converts a DynamoDB Record response into an Adtech object
 * Precondition: DynamoDB Record must contain all Adtech properties
 * @param {Record} dynamodbResponse - Adtech entry received from dynamoDB
 */
export const converter = (
  dynamodbResponse: Record<string, NativeAttributeValue>,
): Adtech => {
  return {
    iamRoleArn: dynamodbResponse.iam_role ?? "",
    iamRoleName: dynamodbResponse.iam_role_name ?? "",
    originURL: dynamodbResponse.origin_url ?? "",
    adtechID: dynamodbResponse.adtech_id ?? "",
    awsID: dynamodbResponse.aws_id ?? "",
  };
};
