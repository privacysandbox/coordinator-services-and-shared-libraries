// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * These are the imports for the generated code based on the protos defined in
 *
 *      examples/proto_in_hooks/proto/collection_of_doubles.proto
 *
 * Note that the namespace is the package specified in the .proto file,
 * but the namespace is also prefix with proto.
 */
goog.require("proto.google.scp.roma.test.proto.CollectionOfDoublesProto");
goog.require("proto.google.scp.roma.test.proto.ListOfDoubleProto");

/**
 * Function we want to be able to call from Roma.
 *
 * The Closure Compiler will see the @export JSDoc annotation below and
 * generate the following synthetic code in the global scope:
 *
 *      goog.exportSymbol('RomaHandlerSendBytes', RomaHandlerSendBytes);
 *
 * That makes the minified version of this function still available in the
 * global namespace.
 *
 * You can also use @export for property names on classes, to prevent them
 * from being minified.
 *
 * @export
 */
function RomaHandlerSendBytes() {
    // Create a proto object and populate it with data
    var list_of_doubles1 = new proto.google.scp.roma.test.proto.ListOfDoubleProto();
    list_of_doubles1.addData(0.1);
    list_of_doubles1.addData(0.22);
    list_of_doubles1.addData(0.333);

    var list_of_doubles2 = new proto.google.scp.roma.test.proto.ListOfDoubleProto();
    list_of_doubles2.addData(0.9);
    list_of_doubles2.addData(0.1010);

    var collection_of_doubles = new proto.google.scp.roma.test.proto.CollectionOfDoublesProto();
    collection_of_doubles.getMetadataMap().set("a key", "a value");
    collection_of_doubles.addData(list_of_doubles1);
    collection_of_doubles.addData(list_of_doubles2);

    // Serialize the proto object
    var proto_as_bytes = collection_of_doubles.serializeBinary();

    // Send the serialized proto object to the C++ side
    send_proto_bytes_to_cpp(proto_as_bytes);

    // Return dummy string to assert in Roma
    return "Hello there from closure-compiled JS :)";
}

/**
 * Function we want to be able to call from Roma.
 *
 * The Closure Compiler will see the @export JSDoc annotation below and
 * generate the following synthetic code in the global scope:
 *
 *   goog.exportSymbol('RomaHandlerGetBytes', RomaHandlerGetBytes);
 *
 * That makes the minified version of this function still available in the
 * global namespace.
 *
 * You can also use @export for property names on classes, to prevent them
 * from being minified.
 *
 * @export
 */
function RomaHandlerGetBytes() {
    // Get the bytes from the C++ side
    // The annotation below is meaningful since the compiler doesn't know the return type
    // of this function and it doesn't know the type of proto_as_bytes. So we tell it to
    // not worry about the return type and to treat proto_as_bytes as an Array
    /**
     * @suppress {reportUnknownTypes}
     * @type {Array}
    */
    var proto_as_bytes = get_proto_bytes_from_cpp();
    // Deserialize the bytes into the proto
    var collection_of_doubles = proto.google.scp.roma.test.proto.CollectionOfDoublesProto.deserializeBinary(proto_as_bytes);

    // Turn the proto into a JS object and return it to be able to assert it on the C++ side
    return collection_of_doubles.toObject();
}
