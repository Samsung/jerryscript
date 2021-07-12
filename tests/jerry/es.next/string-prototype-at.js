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

var str = 'This is an Apple';

var index = str.at(0);
assert(index === 'T');
assert(str[index] === undefined);

assert(str.at(str.length) === undefined);
assert(str.at(str.length+1) === undefined);
assert(str.at(str.length-1) === 'e');
assert(str.at("1") === 'h');
assert(str.at(-1) === 'e');
assert(str.at("-1") === 'e');
assert(str.at("-20") === undefined);

try {
  String.prototype.at.call(undefined)
  assert (false);
} catch(e) {
  assert(e instanceof TypeError);
}

var obj = {toString: function() { return "Apple"; } };
obj.at = String.prototype.at;
assert(obj.at(0) === 'A');
