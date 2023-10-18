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

# Do not change the definition of role_name!
locals {
  policy_name = "${var.remote_environment}_pbs_remote_role_policy_${var.remote_coordinator_aws_account_id}"
  role_name   = "${var.remote_environment}_pbs_remote_role_${var.remote_coordinator_aws_account_id}"
}

# Policy to grant the remote coordinator access to communicate with this coordinater.
resource "aws_iam_policy" "remote_coordinator_assume_role_policy" {
  name        = local.policy_name
  path        = "/"
  description = "IAM policy to allow principal ${var.remote_coordinator_aws_account_id} to authenticate against PBS."

  policy = <<EOF
{
  "Statement": [
    {
      "Action": [
        "execute-api:Invoke"
      ],
      "Effect": "Allow",
      "Resource": "${var.pbs_auth_api_gateway_arn}"
    }
  ],
  "Version": "2012-10-17"
}
EOF
}

# Do not change the name of this role!
resource "aws_iam_role" "remote_coordinator_assume_role" {
  name = local.role_name
  assume_role_policy = jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        Action    = "sts:AssumeRole"
        Condition = {}
        Effect    = "Allow"
        Principal = {
          AWS = var.remote_coordinator_aws_account_id
        }
      },
    ]
  })
}

# Attaches the policy to the role
resource "aws_iam_role_policy_attachment" "remote_coordinator_assume_role_policy" {
  role       = aws_iam_role.remote_coordinator_assume_role.name
  policy_arn = aws_iam_policy.remote_coordinator_assume_role_policy.arn
}
