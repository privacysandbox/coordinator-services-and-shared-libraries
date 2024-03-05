# Copyright 2023 Google LLC
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

import os
import urllib.parse

from aws_requests_auth.aws_auth import AWSRequestsAuth
from aws_synthetics.common import synthetics_logger as logger
import boto3
import requests

# UPDATE THE URL TO PROBE
_URL_TO_PROBE = os.environ.get("URL_TO_PROBE")
_COORDINATOR_ROLE = os.environ.get("ALLOWED_ASSUME_ROLE")
_AWS_REGION = os.environ.get("AWS_REGION")


def verify_request(method, url, coordinator_role, aws_region):
  """Verifies the requests response is as expected.

  Arguments:
    method: string, HTTP method to use for the request,
    url: string, URL to send the request to.
    coordinator_role: string, Coordinator Role used to authenticate to API.
    aws_region: string, AWS Region used to create session token.
  """
  parsed_url = urllib.parse.urlparse(url)

  sts_client = boto3.client("sts")
  assumed_role_object = sts_client.assume_role(
      RoleArn=coordinator_role,
      RoleSessionName="coordinator_assume_role",
  )
  credentials = assumed_role_object["Credentials"]
  auth = AWSRequestsAuth(
      aws_access_key=credentials["AccessKeyId"],
      aws_secret_access_key=credentials["SecretAccessKey"],
      aws_token=credentials["SessionToken"],
      aws_host=parsed_url.hostname,
      aws_region=aws_region,
      aws_service="execute-api",
  )

  logger.info(f"Making request with Method: '{method}' URL: {url}:")

  response = requests.get(url, auth=auth)

  if (
      not response.status_code
      or response.status_code < 200
      or response.status_code > 299
  ):
    try:
      logger.error(f"Response: {response.text}")
    finally:
      if response.reason:
        response.close()
        raise Exception(f"Failed: {response.reason}")
      else:
        response.close()
        raise Exception(f"Failed with status code: {response.status_code}")

  logger.info("HTTP request successfully executed.")
  logger.info(f"Status Code: {response.status_code}")
  logger.info(f"Response: {response.text}")


def main():
  verify_request("GET", _URL_TO_PROBE, _COORDINATOR_ROLE, _AWS_REGION)
  logger.info("Canary successfully executed.")


def handler(event, context):
  logger.info("Private Key Heartbeat Python synthetic canary.")
  main()
