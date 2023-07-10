/**
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

variable "instance_name" {
  description = "Value of the Name tag for the EC2 instance."
  type        = string
  default     = "ExampleCpioTestInstance"
}

variable "ami" {
  description = "Value of AMI ID to bring up the EC2 instance."
  type        = string
  default     = ""
}

variable "iam_role" {
  description = "Value of the IAM role to assign to the EC2 instance. Needed to run binary inside TEE."
  type        = string
  default     = ""
}

variable "key_pair_name" {
  description = "Value of the key pair name to assign to the EC2 instance. Needed to SSH to the EC2 instance."
  type        = string
  default     = ""
}

variable "subnet_id" {
  description = "Value of the subnet ID to assign to the EC2 instance."
  type        = string
  default     = ""
}
