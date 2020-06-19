// Copyright JS Foundation and other contributors, http://js.foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

var float_array = new Float32Array([ 1.125, 5.5, -1.25, -0.0 ]);
var int_array = new Int8Array([ 3, 2, 1, 100, -30 ]);
var uint_array = new Uint8Array([ 3, 2, 1, 100, -30 ]);
var empty_array = new Uint32Array();

assert((JSON.stringify(float_array)) === '{"0":1.125,"1":5.5,"2":-1.25,"3":0}');
assert((JSON.stringify(int_array)) === '{"0":3,"1":2,"2":1,"3":100,"4":-30}');
assert((JSON.stringify(uint_array)) === '{"0":3,"1":2,"2":1,"3":100,"4":226}');
assert((JSON.stringify(empty_array)) === '{}');
