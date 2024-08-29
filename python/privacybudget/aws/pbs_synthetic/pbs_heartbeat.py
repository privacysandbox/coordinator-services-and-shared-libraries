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
from aws_synthetics.common import synthetics_logger as logger
from aws_synthetics.selenium import synthetics_webdriver as syn_webdriver
from hyper.contrib import HTTP20Adapter
import requests


# UPDATE THE URL TO PROBE
_URL_TO_PROBE = os.environ.get("URL_TO_PROBE") + ":status"


def verify_request(url, headers):
  """Verifies the requests response is as expected.

  Arguments:
    url: string, URL to send the request to.
    headers: dict, adds optional headers to request.

  Raises:
    Exception: If any problem parsing response we raise an exception.
  """
  user_agent = str(syn_webdriver.get_canary_user_agent_string())
  if "User-Agent" in headers:
    headers["User-Agent"] = f"{user_agent} {headers['User-Agent']}"
  else:
    headers["User-Agent"] = user_agent

  logger.info(f"Making get request with URL:headers {url}:{headers}")

  session = requests.Session()
  session.mount("https://", HTTP20Adapter())
  response = session.get(url, headers=headers)

  if not response.status_code or response.status_code != 403:
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
  logger.info(f"Reason: {response.reason}")
  logger.info(f"Response Headers: {response.headers}")
  response.close()


def main():
  headers = {"x-auth-token": "invalid", "content-length": "0"}
  verify_request(_URL_TO_PROBE, headers)
  logger.info("Canary successfully executed.")


def handler(event, context):
  logger.info("PBS Heartbeat Python synthetic canary.")
  main()
