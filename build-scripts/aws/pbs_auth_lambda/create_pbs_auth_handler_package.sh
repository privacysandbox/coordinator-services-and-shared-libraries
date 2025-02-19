#!/bin/bash
# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


set -euo pipefail

# Capture inputs into variables
SOURCE_FILE=$1
REQUIREMENTS_FILE=$2
OUTPUT_FILE=$3

# Create a temporary directory to hold our lambda package
temp_dir="lambda_pkg_dir"
mkdir $temp_dir

# Create and activate a new virtual environment
python -m venv ./auth_handler_venv
source ./auth_handler_venv/bin/activate

# Install pip dependencies
if [ -f "$REQUIREMENTS_FILE" ]; then
  echo "Installing dependencies from requirements.txt"
  pip install -r "$REQUIREMENTS_FILE"
else
  echo "Error: requirement.txt is missing."
fi

# Deactivate the virtual environment
deactivate

# Pip dependencies are installed in a folder under the directory of pythong runtime.
# We do not know the exact python runtime version of virtual env ahead of time.
# We need to derive it by listing the folder for that runtime.
python_runtime=$(ls ./auth_handler_venv/lib/)

# Collect the handler source file along with its dependencies in our temporary directory and zip it.
cp -r ./auth_handler_venv/lib/$python_runtime/site-packages/* ./$temp_dir
cp -r $SOURCE_FILE ./$temp_dir
cd ./$temp_dir
zip -r ../$OUTPUT_FILE *
cd ..

# Clean up the virtual environment and the temporary directory
rm -rf ./auth_handler_venv
rm -rf ./$temp_dir