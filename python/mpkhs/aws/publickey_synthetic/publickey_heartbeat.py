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

import http.client
import json
import os
import urllib.parse
from aws_synthetics.common import synthetics_logger as logger
from aws_synthetics.selenium import synthetics_webdriver as syn_webdriver

# UPDATE THE URL TO PROBE
_URL_TO_PROBE = os.environ.get("URL_TO_PROBE")


def verify_request(method, url, post_data=None, headers={}):
  """Verifies the requests response is as expected.

  Arguments:
    method: string, HTTP Method to use, i.e. GET, POST...
    url: string, URL to send the request to.
    post_data: string, (Optional) Any data needed post to make "POST" request.
    headers: dict, (Optional) adds optional headers to request.

  Raises:
    Exception: If any problem parsing response we raise an exception.
  """
  parsed_url = urllib.parse.urlparse(url)
  user_agent = str(syn_webdriver.get_canary_user_agent_string())
  if "User-Agent" in headers:
    headers["User-Agent"] = f"{user_agent} {headers['User-Agent']}"
  else:
    headers["User-Agent"] = user_agent

  logger.info(
      f"Making request with Method: '{method}' URL: {url}: Data:"
      f" {json.dumps(post_data)} Headers: {json.dumps(headers)}"
  )

  if parsed_url.scheme == "https":
    conn = http.client.HTTPSConnection(parsed_url.hostname, parsed_url.port)
  else:
    conn = http.client.HTTPConnection(parsed_url.hostname, parsed_url.port)

  conn.request(method, url, post_data, headers)
  response = conn.getresponse()
  logger.info(f"Status Code: {response.status}")
  logger.info(f"Response Headers: {json.dumps(response.headers.as_string())}")

  if not response.status or response.status < 200 or response.status > 299:
    try:
      logger.error(f"Response: {response.read().decode()}")
    finally:
      if response.reason:
        conn.close()
        raise Exception(f"Failed: {response.reason}")
      else:
        conn.close()
        raise Exception(f"Failed with status code: {response.status}")

  # Inspect response ane make sure we have 5 keys.
  try:
    keys = json.loads(response.read())
    if len(keys["keys"]) != 5:
      conn.close()
      logger.error(f"Response: {keys}")
      raise Exception(f"Failed: response not as expected.")
  finally:
    logger.info(f"Response: {keys}")

  logger.info("HTTP request successfully executed.")
  conn.close()


def main():
  verify_request("GET", _URL_TO_PROBE)
  logger.info("Canary successfully executed.")


def handler(event, context):
  logger.info("Public Key Heartbeat Python synthetic canary.")
  main()
