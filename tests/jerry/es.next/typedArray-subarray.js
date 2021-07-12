/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var a = new Int32Array([1, 2, 3, 4, 5]);
assert(a.subarray().toString() === '1,2,3,4,5');
assert(a.subarray(3).toString() === '4,5');
assert(a.subarray(1, 3).toString() === '2,3');
assert(a.subarray(-3).toString() === '3,4,5');
assert(a.subarray(-3, -1).toString() === '3,4');
assert(a.subarray(3, 2).toString() === '');
assert(a.subarray(-2, -3).toString() === '');
assert(a.subarray(4, 1).toString() === '');
assert(a.subarray(-1, -4).toString() === '');
assert(a.subarray(1).subarray(1).toString() === '3,4,5');
assert(a.subarray(1, 4).subarray(1, 2).toString() === '3');

var b = new Uint8Array([]);
assert(b.subarray(123456, -123456).toString() === '');
assert(b.subarray().subarray().toString() === '');

var ab = new ArrayBuffer(28);
var tmp = new Int32Array(ab);
tmp.set([0, 1, 2, 3, 4, 5, 0]);
var c = new Int32Array(ab, 4, 5);
assert(c.toString() === '1,2,3,4,5');
assert(c.subarray().toString() === '1,2,3,4,5');
assert(c.subarray(3).toString() === '4,5');
assert(c.subarray(1, 3).toString() === '2,3');
assert(c.subarray(-3).toString() === '3,4,5');
assert(c.subarray(-3, -1).toString() === '3,4');
assert(c.subarray(3, 2).toString() === '');
assert(c.subarray(-2, -3).toString() === '');
assert(c.subarray(4, 1).toString() === '');
assert(c.subarray(-1, -4).toString() === '');
assert(c.subarray(1).subarray(1).toString() === '3,4,5');
assert(c.subarray(1, 4).subarray(1, 2).toString() === '3');

var cd = new SharedArrayBuffer(28);
tmp = new Int32Array(cd);
tmp.set([0, 1, 2, 3, 4, 5, 0]);
var d = new Int32Array(cd, 4, 5);
assert(d.subarray().toString() === '1,2,3,4,5');
assert(d.subarray(3).toString() === '4,5');
assert(d.subarray(1, 3).toString() === '2,3');
assert(d.subarray(1, 3).toString() === '2,3');
assert(d.subarray(-3).toString() === '3,4,5');
assert(d.subarray(-3, -1).toString() === '3,4');
assert(d.subarray(3, 2).toString() === '');
assert(d.subarray(-2, -3).toString() === '');
assert(d.subarray(4, 1).toString() === '');
assert(d.subarray(-1, -4).toString() === '');
assert(d.subarray(1).subarray(1).toString() === '3,4,5');
assert(d.subarray(1, 4).subarray(1, 2).toString() === '3');
