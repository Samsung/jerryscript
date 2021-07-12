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

var normal_typedarrays = [
  new Int8Array([0, 1, 2, 3, 0, 5, 6]),
  new Uint8Array([0, 1, 2, 3, 0, 5, 6]),
  new Uint8ClampedArray([0, 1, 2, 3, 0, 5, 6]),
  new Int16Array([0, 1, 2, 3, 0, 5, 6]),
  new Uint16Array([0, 1, 2, 3, 0, 5, 6]),
  new Int32Array([0, 1, 2, 3, 0, 5, 6]),
  new Uint32Array([0, 1, 2, 3, 0, 5, 6]),
  new Float32Array([0.5, 1.5, 2.5, 3.5, 0.5, 5.5, 6.5]),
  new Float64Array([1.5, 2.5, 3.5, 0.5, 5.5, 6.5, 2.5]),
  new BigInt64Array([1n, 10n, 2n, 3n, 4n, 5n, 6n]),
  new BigUint64Array([3n, 1n, 2n, 3n, 33n, 5n, 6n])
];

var index = normal_typedarrays[0].at(0);
assert(index === 0);
assert(normal_typedarrays[index].at(0) === 0);

assert(normal_typedarrays[1].at(normal_typedarrays[1].length) === undefined);
assert(normal_typedarrays[2].at(normal_typedarrays[2].length+1) === undefined);
assert(normal_typedarrays[3].at(normal_typedarrays[3].length-1) === 6);
assert(normal_typedarrays[4].at("1") === 1);
assert(normal_typedarrays[5].at(-1) === 6);
assert(normal_typedarrays[6].at("-1") === 6);
assert(normal_typedarrays[7].at("-20") === undefined);
assert(normal_typedarrays[8].at(0) === 1.5);
assert(normal_typedarrays[9].at(-1) === 6n);
assert(normal_typedarrays[10].at() === 3n);

normal_typedarrays.forEach(function(e){
  try {
    e.prototype.at.call(undefined);
    assert(false);
  } catch(e) {
    assert(e instanceof TypeError);
  }
});
