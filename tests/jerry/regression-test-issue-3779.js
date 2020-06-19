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

var array = [ 1, 2, 3 ];
var idx_50 = array.lastIndexOf(50, {
  valueOf: function() {
    // Trigger removing elements from fast array.
    array.length = 0;
  },
});

assert(idx_50 === -1);

var idx_51 = array.lastIndexOf(51, {
  valueOf: function() {
    array.push(51);
    return 10;
  },
});

assert(idx_51 === -1);
