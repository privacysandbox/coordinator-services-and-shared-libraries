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

cc_library(
    name = "xml2",
    #srcs = glob(["*.c"]),
    srcs = [
        "HTMLparser.c",
        "HTMLtree.c",
        "SAX.c",
        "SAX2.c",
        "buf.c",
        "c14n.c",
        "catalog.c",
        "chvalid.c",
        "debugXML.c",
        "dict.c",
        "encoding.c",
        "entities.c",
        "error.c",
        "globals.c",
        "hash.c",
        "legacy.c",
        "list.c",
        "nanoftp.c",
        "nanohttp.c",
        "parser.c",
        "parserInternals.c",
        "pattern.c",
        "relaxng.c",
        "schematron.c",
        "threads.c",
        "tree.c",
        "uri.c",
        "valid.c",
        "xinclude.c",
        "xlink.c",
        "xmlIO.c",
        "xmlmemory.c",
        "xmlmodule.c",
        "xmlreader.c",
        "xmlregexp.c",
        "xmlsave.c",
        "xmlschemas.c",
        "xmlschemastypes.c",
        "xmlstring.c",
        "xmlunicode.c",
        "xmlwriter.c",
        "xpath.c",
        "xpointer.c",
        "xzlib.c",
    ],
    hdrs = glob([
        "*.h",
        "include/libxml/*.h",
    ]),
    copts = [
        "-D_REENTRANT",
        "-DHAVE_CONFIG_H",
        "-w",
    ],
    includes = [
        ".",
        "include",
    ],
    linkopts = [
        "-pthread",
        "-Wl,-rpath,\\$$ORIGIN/../lib",
    ],
    linkstatic = False,
    textual_hdrs = ["trionan.c"],
    visibility = ["//visibility:public"],
    deps = [
        "@madler_zlib//:zlib",
    ],
)
