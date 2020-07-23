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

/* Adding property to a frozen object */
var a = {one: "test"};
a.two = 3;
Object.freeze(a);
a.three = 7;
assert(a.three === undefined);

/* Adding properties to frozen global object */
Object.freeze(this);
assert(eval ('function b() {};') === undefined);
assert(eval('var test_var = 3') === undefined);

/* Check strict mode TypeError */
function fail() {
  'use strict';
  a.one = 'test'; // throws a TypeError
  delete a.two; // throws a TypeError
  a.three = 'test2'; // throws a TypeError
}

try {
  fail();
} catch (e) {
  assert(e instanceof TypeError);
}

function fail_two() {
  'use strict';
  this.a = 'test';
}

try {
  fail_two();
} catch (e) {
  assert(e instanceof TypeError);
}
/* Check properties of a */
assert(Object.keys(a) == "one,two");
/* Check properties of global object */
assert(Object.keys(this) == "assert,gc,print,resourceName,Assert,a,fail,fail_two");
