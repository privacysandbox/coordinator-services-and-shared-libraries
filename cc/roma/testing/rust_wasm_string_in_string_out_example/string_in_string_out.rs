/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#![no_main]

// String representation to define memory layout
#[repr(C)]
pub struct StringRepresentation {
    string: *const u8,
    length: u32,
}

// Helper function to turn a StringRepresentation to a native rust String
fn string_representation_to_string(input: *const StringRepresentation)
    -> String {
    let input_as_rust_str: String;

    unsafe {
        let slice = std::slice::from_raw_parts(
            (*input).string,
            (*input).length as usize);
        input_as_rust_str =
            String::from_utf8(slice.to_vec()).unwrap().to_owned();
    }

    return input_as_rust_str;
}

// Helper function to turn a native String to a StringRepresentation
// so that it can be returned and accessible outside of the WASM block.
fn string_to_string_representation(input: &String)
    -> *const StringRepresentation {
    let mut buffer = input.clone().into_bytes().into_boxed_slice();
    let out_data = buffer.as_mut_ptr();
    let out_len = input.len() as u32;

    // Leak this memory on purpose. This needs to remain in the heap so that it
    // can be accessible as a return value.
    std::mem::forget(buffer);

    let string_representation = StringRepresentation {
        string: out_data,
        length: out_len,
    };

    return Box::into_raw(Box::new(string_representation));
}

// No mangle to ensure a predictable handler name
#[no_mangle]
pub extern "C" fn Handler(input: *const StringRepresentation)
    -> *const StringRepresentation {
    // Turn the string representation to a native string
    let mut input_as_rust_str = string_representation_to_string(input);

    // Add to the string to return it
    input_as_rust_str.push_str(" Hello from rust!");

    // Turn the native string back into a string representation
    let str_ptr = string_to_string_representation(&input_as_rust_str);

    return str_ptr;
}
