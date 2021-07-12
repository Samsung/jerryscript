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

var obj = {};
var array = ['Apple', 'Banana', "zero", 0, obj, 'Apple'];

var index = array.at(0);
assert(index === 'Apple');
assert(array[index] === undefined);

assert(array.at(array.length) === undefined);
assert(array.at(array.length+1) === undefined);
assert(array.at(array.length-1) === 'Apple');
assert(array.at("1") === 'Banana');
assert(array.at(-1) === 'Apple');
assert(array.at("-1") === 'Apple');
assert(array.at("-20") === undefined);

/* 3 */
try {
  array.at({valueOf(){
    throw new ReferenceError ("foo");
  }})
} catch(e) {  
  assert(e.message === "foo");
  assert(e instanceof ReferenceError);
}

/* 7 */
var obj = {}
obj.length = 1;
Object.defineProperty(obj, '0', { 'get' : function () {throw new ReferenceError ("foo"); } });
obj.at = Array.prototype.at;

try {
  obj.at(0);
  assert(false);
} catch(e) {
  assert(e.message === "foo");
  assert(e instanceof ReferenceError);
}
