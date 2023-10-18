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

// List of string representation to define memory layout
#[repr(C)]
pub struct ListOfStringRepresentation {
    // A C-like array of pointers
    list: *const *const StringRepresentation,
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

// Helper function to turn a ListOfStringRepresentation into a Vec<String>
fn list_of_string_representation_to_vec_of_string(
    input: *const ListOfStringRepresentation,
) -> Vec<String> {
    let input_len: u32;
    unsafe {
        input_len = (*input).length;
    }
    let mut input_as_rust_vec: Vec<String> =
        Vec::with_capacity(input_len as usize);
    for i in 0..input_len {
        unsafe {
            let ptr = (*input).list;
            input_as_rust_vec.push(string_representation_to_string(
                *ptr.wrapping_offset(i as isize).to_owned(),
            ));
        }
    }

    return input_as_rust_vec;
}

// Helper function to turn a Vec<String> into a ListOfStringRepresentation
// so that it can be returned and accessible outside of the WASM.
fn vec_of_string_to_list_of_string_representation(
    input: &Vec<String>,
) -> *const ListOfStringRepresentation {
    let mut result_list = Vec::with_capacity(input.len());

    for s in input {
        let str_ptr = string_to_string_representation(&s);
        result_list.push(str_ptr);
    }

    let mut buffer = result_list.into_boxed_slice();
    let out_data = buffer.as_mut_ptr();
    let out_len = buffer.len() as u32;
    // Leak this memory on purpose. This needs to remain in the heap so that it
    // can be accessible as a return value.
    std::mem::forget(buffer);

    let list_representation = ListOfStringRepresentation {
        list: out_data,
        length: out_len,
    };

    return Box::into_raw(Box::new(list_representation));
}

// No mangle to ensure a predictable handler name
#[no_mangle]
pub extern "C" fn Handler(
    input: *const ListOfStringRepresentation,
) -> *const ListOfStringRepresentation {
    // Turn the list of string representation into a vector of string
    let mut input_as_rust_vec =
        list_of_string_representation_to_vec_of_string(input);

    // Add to the string to return it
    input_as_rust_vec.push("Hello from rust1".to_string());
    input_as_rust_vec.push("Hello from rust2".to_string());

    // Turn the vector of string back into a list of string representation
    let list_ptr =
        vec_of_string_to_list_of_string_representation(&input_as_rust_vec);

    return list_ptr;
}
