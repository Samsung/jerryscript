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
assert(a.fill(0).toString() === '0,0,0,0,0');
assert(a.toString() === '0,0,0,0,0');
assert(a.fill(1, 3).toString() === '0,0,0,1,1');
assert(a.toString() === '0,0,0,1,1');
assert(a.fill(2, 1, 3).toString() === '0,2,2,1,1');
assert(a.toString() === '0,2,2,1,1');
assert(a.fill(3, -3).toString() === '0,2,3,3,3');
assert(a.toString() === '0,2,3,3,3');
assert(a.fill(4, -3, -1).toString() === '0,2,4,4,3');
assert(a.toString() === '0,2,4,4,3');
assert(a.fill(5, 3, 2).toString() === '0,2,4,4,3');
assert(a.toString() === '0,2,4,4,3');
assert(a.fill(6, -2, -3).toString() === '0,2,4,4,3');
assert(a.toString() === '0,2,4,4,3');
assert(a.fill(7, 4, 1).toString() === '0,2,4,4,3');
assert(a.toString() === '0,2,4,4,3');
assert(a.fill(8, -1, -4).toString() === '0,2,4,4,3');
assert(a.toString() === '0,2,4,4,3');
assert(a.fill(9, 1).fill(10, 1).toString() === '0,10,10,10,10');
assert(a.toString() === '0,10,10,10,10');
assert(a.fill(11, 0, 4).fill(12, 1, 2).toString() === '11,12,11,11,10');
assert(a.toString() === '11,12,11,11,10');
assert(a.fill(13, 999, 1000).fill(14, -1000, -999).toString() === '11,12,11,11,10');
assert(a.toString() === '11,12,11,11,10');
assert(a.fill(14, 0, 0).toString() === '11,12,11,11,10');
assert(a.toString() === '11,12,11,11,10');
assert(a.fill(15, a.length, a.length).toString() === '11,12,11,11,10');
assert(a.toString() === '11,12,11,11,10');
assert(a.fill(NaN).toString() === '0,0,0,0,0'); // NaN gets coerced into an integer.
assert(a.toString() === '0,0,0,0,0');
assert(a.fill({ valueOf: () => 16 }).toString() === '16,16,16,16,16');
assert(a.toString() === '16,16,16,16,16');

var b = new Uint8Array();
assert(b.fill(1).toString() === '');
assert(b.toString() === '');
assert(b.fill(2, 0, 0).toString() === '');
assert(b.toString() === '');
assert(b.fill(3, b.length, b.length).toString() === '');
assert(b.toString() === '');

var c = new Uint8Array([0]);
assert(c.fill(256).toString() === '0');
assert(c.toString() === '0');
assert(c.fill(257).toString() === '1');
assert(c.toString() === '1');

try {
  c.fill({});
} catch (e) {
  assert(e instanceof TypeError);
  assert(c.toString() === '1');
}

var d = new Float32Array([0]);
assert(d.fill(NaN).toString() === 'NaN');
assert(d.toString() === 'NaN');

var ab = new ArrayBuffer(4);
var e = new Uint8Array(ab);
assert(e.fill(0).toString() === '0,0,0,0');

var f = new Uint32Array(ab);
assert(f.fill(1).toString() === '1');
assert(e.toString() === '1,0,0,0');

var g = new Uint8Array(ab, 1, 2);
assert(g.toString() === '0,0');
assert(g.fill(2).toString() === '2,2');
assert(e.toString() === '1,2,2,0');
assert(g.fill(3, -1).toString() === '2,3');
assert(e.toString() === '1,2,3,0');
assert(g.fill(4, 0, 2).toString() === '4,4');
assert(e.toString() === '1,4,4,0');
assert(g.fill(5, 0, 999).toString() === '5,5');
assert(e.toString() === '1,5,5,0');
assert(g.fill(6, -999, 999).toString() === '6,6');
assert(e.toString() === '1,6,6,0');
assert(g.fill(7, -999, 0).toString() === '6,6');
assert(e.toString() === '1,6,6,0');

var ab2 = new ArrayBuffer(4);
var h = new Uint8Array(ab2);
var i = new Uint16Array(ab2, 0, 1);
assert(i.fill(1).toString() === '1');
assert(h.toString() === '1,0,0,0');
var j = new Uint16Array(ab2, 2, 1);
assert(j.fill(1).toString() === '1');
assert(h.toString() === '1,0,1,0');

var k = new BigInt64Array([1n, 2n, 3n, 4n, 5n]);
assert(k.fill(1n, 1, 4).toString() === '1,1,1,1,5');
assert(k.fill(18446744073709551616n, 0, 5).toString() === '0,0,0,0,0');
assert(k.fill(-5n, 3, 5).toString() === '0,0,0,-5,-5');

var l = new BigUint64Array([1n, 2n, 3n, 4n, 5n]);
assert(l.fill(-18446744073709551614n, 3, 5).toString() === '1,2,3,2,2');
assert(l.fill(18446744073709551614n, 4).toString() === '1,2,3,2,18446744073709551614');

var cd = new SharedArrayBuffer(4);
var u8array = new Uint8Array(cd);
assert(u8array.fill(0).toString() === '0,0,0,0');

var invalid = {
  valueOf: function() {
    throw new Error();
  }
};

var n = new BigInt64Array();
try {
  n.fill(1n, invalid);
  assert(false)
} catch (e) {
  assert(e instanceof Error);
}

try {
  n.fill(1n, 0, invalid);
  assert(false)
} catch (e) {
  assert(e instanceof Error);
}
