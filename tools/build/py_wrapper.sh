#!/bin/bash
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


set -euo pipefail

# Use python 3.7 if it is installed under /usr/local/bin, because system
# python may be old.
KOKORO_PY37_PATH=/usr/local/bin/python3.7

if [[ -x $KOKORO_PY37_PATH ]]; then
  exec $KOKORO_PY37_PATH "$@"
else
  exec python3 "$@"
fi
