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
  AttachRolePolicyCommand,
  AttachRolePolicyCommandOutput,
  CreateRoleCommand,
  CreateRoleCommandOutput,
  DeleteRoleCommand,
  DeleteRoleCommandOutput,
  DetachRolePolicyCommand,
  DetachRolePolicyCommandOutput,
  IAMClient,
  Role,
} from "@aws-sdk/client-iam";
const iamClient = new IAMClient({});
/**
 * Creates IAM role for an Adtech in IAM
 * @param {string} adtechAWSId - AWS account ID for an Adtech
 * @returns {CreateRoleCommandOutput}
 */
export const createIAMRole = async (adtechAWSId: string): Promise<Role> => {
  const command = new CreateRoleCommand({
    AssumeRolePolicyDocument: JSON.stringify({
      Version: "2012-10-17",
      Statement: [
        {
          Effect: "Allow",
          Principal: {
            AWS: [`arn:aws:iam::${adtechAWSId}:root`],
          },
          Action: "sts:AssumeRole",
        },
      ],
    }),
    RoleName: `coordinator-role-${adtechAWSId}`,
  });
  const response: CreateRoleCommandOutput = await iamClient.send(command);
  return response.Role!;
};
/**
 * Deletes IAM role for an Adtech in IAM
 * @param {string} iamRoleName - Name of Adtech's IAM role
 * @returns {DeleteRoleCommandOutput}
 */
export const deleteIAMRole = async (
  iamRoleName: string,
): Promise<DeleteRoleCommandOutput> => {
  const command = new DeleteRoleCommand({ RoleName: iamRoleName });
  return iamClient.send(command);
};
/**
 * Attaches a policy to a role
 * @param {string} policyARN - AWS ARN for Coordinator policy to be attached to role
 * @param {string} roleName - Name of Adtech's role to receive the policy
 * @returns {AttachRolePolicyCommandOutput}
 */
export const attachRolePolicy = async (
  roleName: string,
): Promise<AttachRolePolicyCommandOutput> => {
  const command = new AttachRolePolicyCommand({
    PolicyArn: process.env.POLICY_ARN,
    RoleName: roleName,
  });
  return iamClient.send(command);
};
/**
 * Detaches a policy from a role
 * @param {string} policyARN - policyARN to detach from role
 * @param roleName - Adtech's role's name
 * @returns {DetachRolePolicyCommandOutput}
 */
export const detachRolePolicy = async (
  roleName: string,
): Promise<DetachRolePolicyCommandOutput> => {
  const command = new DetachRolePolicyCommand({
    PolicyArn: process.env.POLICY_ARN,
    RoleName: roleName,
  });
  return iamClient.send(command);
};
