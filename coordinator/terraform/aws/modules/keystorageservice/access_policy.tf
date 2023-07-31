# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Grants access to Coordinator A role's KeyGeneration process.

locals {
  # What condition policy to add to key_policy.
  condition_policy = length(var.keygeneration_attestation_condition_keys) > 0 ? {
    Condition = {
      StringEqualsIgnoreCase = var.keygeneration_attestation_condition_keys
    }
  } : {}

  # By merging the two objects we are setting "Condition" if it is present, or
  # not adding a condition if there is no condition to add.
  key_policy = merge({
    Action = [
      "kms:Decrypt"
    ],
    Effect   = "Allow",
    Resource = aws_kms_key.data_key_encryptor.arn
  }, local.condition_policy)

  api_policy_statement = {
    Action = [
      "execute-api:Invoke"
    ],
    Effect = "Allow",
    Resource = [
      #        all regions ↓|↓ all dest accounts    |↓ all stages
      "arn:aws:execute-api:*:*:${var.api_gateway_id}/*/POST/${local.get_data_key_route}",
      "arn:aws:execute-api:*:*:${var.api_gateway_id}/*/POST/${local.create_key_route}",
    ]
  }
}

# Policy which grants access to a peer to access KeyStorageService and its corresponding keys
resource "aws_iam_policy" "peer_coordinator_policy" {
  name = "${var.environment}_peer_coordinator_keystorage_access"
  policy = jsonencode({
    Version   = "2012-10-17",
    Statement = [local.key_policy, local.api_policy_statement]
  })
}

# Role used by peer coordinator to auth with keystorageservice.
resource "aws_iam_role" "peer_coordinator_assume_role" {
  name = "${var.environment}_peer_coordinator_keygeneration_assume_role"
  assume_role_policy = jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        Action    = "sts:AssumeRole"
        Condition = {}
        Effect    = "Allow"
        Principal = {
          AWS = var.allowed_peer_coordinator_principals
        }
      },
    ]
  })
}

resource "aws_iam_role_policy_attachment" "peer_coordinator_policy" {
  role       = aws_iam_role.peer_coordinator_assume_role.name
  policy_arn = aws_iam_policy.peer_coordinator_policy.arn
}
