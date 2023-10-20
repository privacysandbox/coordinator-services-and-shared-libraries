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

locals {
  signature_key_type      = "ECC_NIST_P256"
  signature_key_algorithm = "ECDSA_SHA_256"
}

module "worker_enclave_encryption_key" {
  source      = "../shared/workerenclaveencryptionkey"
  environment = "tp1-${var.environment}"
}

# Used to sign public EncryptionKeys during key creation
resource "aws_kms_key" "encryption_key_signature_key" {
  count                    = var.enable_public_key_signature ? 1 : 0
  description              = "${var.environment}_encryption_key_signature_key"
  key_usage                = "SIGN_VERIFY"
  customer_master_key_spec = local.signature_key_type
}

resource "aws_kms_alias" "encryption_key_signature_key_alias" {
  count         = var.enable_public_key_signature ? 1 : 0
  name          = "alias/${var.environment}-encryption-key-signature-key"
  target_key_id = aws_kms_key.encryption_key_signature_key[0].key_id
}
