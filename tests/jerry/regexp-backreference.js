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

var r;

r = new RegExp('(a)b\\1').exec('aba');
assert(r[0] == 'aba');
assert(r[1] == 'a');

r = new RegExp('(a)b\\1').exec('b');
assert(r == undefined);

r = new RegExp('(a)*b\\1').exec('b');
assert(r[0] == 'b');
assert(r[1] == undefined);

assert(JSON.stringify(/[[]?(a)\1/.exec('aa')) === '["aa","a"]');
assert(JSON.stringify(/\1{2,5}()\B/.exec('asd')) === '["",""]');
