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

output_target="./string_in_string_out.wasm"
cargo build  --target wasm32-unknown-unknown --release
[ -f $output_target ] && rm $output_target
cp target/wasm32-unknown-unknown/release/$output_target ./
wasm-gc $output_target $output_target
