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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "rapidjson",
    srcs = [
        "include/rapidjson/internal/biginteger.h",
        "include/rapidjson/internal/clzll.h",
        "include/rapidjson/internal/diyfp.h",
        "include/rapidjson/internal/dtoa.h",
        "include/rapidjson/internal/ieee754.h",
        "include/rapidjson/internal/itoa.h",
        "include/rapidjson/internal/meta.h",
        "include/rapidjson/internal/pow10.h",
        "include/rapidjson/internal/regex.h",
        "include/rapidjson/internal/stack.h",
        "include/rapidjson/internal/strfunc.h",
        "include/rapidjson/internal/strtod.h",
        "include/rapidjson/internal/swap.h",
    ],
    hdrs = [
        "include/rapidjson/allocators.h",
        "include/rapidjson/document.h",
        "include/rapidjson/encodedstream.h",
        "include/rapidjson/encodings.h",
        "include/rapidjson/error/en.h",
        "include/rapidjson/error/error.h",
        "include/rapidjson/filereadstream.h",
        "include/rapidjson/filewritestream.h",
        "include/rapidjson/fwd.h",
        "include/rapidjson/istreamwrapper.h",
        "include/rapidjson/memorybuffer.h",
        "include/rapidjson/memorystream.h",
        "include/rapidjson/ostreamwrapper.h",
        "include/rapidjson/pointer.h",
        "include/rapidjson/prettywriter.h",
        "include/rapidjson/rapidjson.h",
        "include/rapidjson/reader.h",
        "include/rapidjson/schema.h",
        "include/rapidjson/stream.h",
        "include/rapidjson/stringbuffer.h",
        "include/rapidjson/writer.h",
    ],
    includes = ["include"],
)
