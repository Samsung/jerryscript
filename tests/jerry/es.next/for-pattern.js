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

var idx = 0;
for (var [ a, b ] of [ [ 1, 2 ], [ 3, 4 ] ]) {
  if (idx == 0) {
    assert(a === 1);
    assert(b === 2);
    idx = 1;
  } else {
    assert(a === 3);
    assert(b === 4);
  }
}

assert(a === 3);
assert(b === 4);

idx = 0;
for (let [ a, b ] of [ [ 5, 6 ], [ 7, 8 ] ]) {
  if (idx == 0) {
    assert(a === 5);
    assert(b === 6);
    idx = 1;
  } else {
    assert(a === 7);
    assert(b === 8);
  }
}

assert(a === 3);
assert(b === 4);

idx = 0;
for (let [ a, b ] of [ [ 11, 12 ], [ 13, 14 ] ]) {
  if (idx == 0) {
    eval('assert(a === 11)');
    eval('assert(b === 12)');
    idx = 1;
  } else {
    eval('assert(a === 13)');
    eval('assert(b === 14)');
  }
}

assert(a === 3);
assert(b === 4);

try {
  eval('for (let [a,b] = [1,2] of [[3,4]]) {}');
  assert(false);
} catch (e) {
  assert(e instanceof SyntaxError);
}
