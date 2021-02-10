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

/* 1. test case */
var d = new Date(2015, 6, 9, 12, 13, 14, 121);

assert (d.getFullYear() == 2015);
assert (d.getUTCFullYear() == 2015);
assert (d.getMonth() == 6);
assert (d.getUTCMonth() == 6);
assert (d.getDate() == 9);
assert (d.getUTCDate() == 9);
assert (d.getDay() == 4);
assert (d.getUTCDay() == 4);
assert (d.getHours() == 12);
assert (d.getUTCHours() == (12 + d.getTimezoneOffset() / 60));
assert (d.getMinutes() == 13);
assert (d.getUTCMinutes() == 13);
assert (d.getSeconds() == 14);
assert (d.getUTCSeconds() == 14);
assert (d.getMilliseconds() == 121);
assert (d.getUTCMilliseconds() == 121);

/* 2. test case */
var d = new Date("2015-07-09T12:13:14.121+01:30");

assert (d.getFullYear() == 2015);
assert (d.getUTCFullYear() == 2015);
assert (d.getMonth() == 6);
assert (d.getUTCMonth() == 6);
assert (d.getDate() == 9);
assert (d.getUTCDate() == 9);
assert (d.getDay() == 4);
assert (d.getUTCDay() == 4);
assert (d.getHours() == Math.floor(12 - 1.5 + d.getTimezoneOffset() / 60));
assert (d.getUTCHours() == Math.floor(12 - 1.5));
assert (d.getMinutes() == 43);
assert (d.getUTCMinutes() == 43);
assert (d.getSeconds() == 14);
assert (d.getUTCSeconds() == 14);
assert (d.getMilliseconds() == 121);
assert (d.getUTCMilliseconds() == 121);

/* 3. test case */
var d = new Date(0);

assert (d.getFullYear() == 1970);
assert (d.getUTCFullYear() == 1970);
assert (d.getMonth() == 0);
assert (d.getUTCMonth() == 0);
assert (d.getDate() == 1);
assert (d.getUTCDate() == 1);
assert (d.getDay() == 4);
assert (d.getUTCDay() == 4);
assert (d.getHours() == 0 - (d.getTimezoneOffset() / 60));
assert (d.getUTCHours() == 0);
assert (d.getMinutes() == 0);
assert (d.getUTCMinutes() == 0);
assert (d.getSeconds() == 0);
assert (d.getUTCSeconds() == 0);
assert (d.getMilliseconds() == 0);
assert (d.getUTCMilliseconds() == 0);

/* 4. test case */
var d = new Date("abcd");
assert (isNaN (d));

assert (isNaN (d.getFullYear()));
assert (isNaN (d.getUTCFullYear()));
assert (isNaN (d.getMonth()));
assert (isNaN (d.getUTCMonth()));
assert (isNaN (d.getDate()));
assert (isNaN (d.getUTCDate()));
assert (isNaN (d.getDay()));
assert (isNaN (d.getUTCDay()));
assert (isNaN (d.getHours()));
assert (isNaN (d.getUTCHours()));
assert (isNaN (d.getMinutes()));
assert (isNaN (d.getUTCMinutes()));
assert (isNaN (d.getSeconds()));
assert (isNaN (d.getUTCSeconds()));
assert (isNaN (d.getMilliseconds()));
assert (isNaN (d.getUTCMilliseconds()));
assert (isNaN (d.getTimezoneOffset()));

/* 5. test case */
assert (new Date(2013, -1).getMonth() === 11);
assert (new Date(-2, -2).getFullYear() === -3);
assert (new Date(30888, 2).getFullYear() === 30888);
assert (new Date(-1, -1).getFullYear() === -2);
assert (new Date(-1, -1, -1).getMonth() === 10);
assert (new Date(-1, -1, -1, -1).getDate() === 28);
assert (new Date(-1, -1, -1, -1, -1).getHours() === 22);
assert (new Date(-1, -1, -1, -1, -1, -1).getMinutes() === 58);
assert (new Date(-1, -1, -1, -1, -1, -1, -1).getSeconds() === 58);
assert (new Date(-1, -1, -1, -1, -1, -1, -1, -1).getMilliseconds() === 999);
