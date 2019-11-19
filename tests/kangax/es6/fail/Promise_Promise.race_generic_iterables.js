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
 *
 * This file is based on work under the following copyright and permission
 * notice:
 *
 *     Copyright (c) 2010-2013 Juriy Zaytsev
 *
 *     Permission is hereby granted, free of charge, to any person obtaining a copy
 *     of this software and associated documentation files (the "Software"), to deal
 *     in the Software without restriction, including without limitation the rights
 *     to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *     copies of the Software, and to permit persons to whom the Software is
 *     furnished to do so, subject to the following conditions:
 *
 *     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *     IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *     AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *     LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *     OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *     SOFTWARE.
 */

var global = this;
global.__createIterableObject = function (arr, methods) {
    methods = methods || {};
    if (typeof Symbol !== 'function' || !Symbol.iterator)
      return {};
    arr.length++;
    var iterator = {
      next: function() {
        return { value: arr.shift(), done: arr.length <= 0 };
      },
      'return': methods['return'],
      'throw': methods['throw']
    };
    var iterable = {};
    iterable[Symbol.iterator] = function(){ return iterator; };
    return iterable;
  };

function test() {

        var fulfills = Promise.race(global.__createIterableObject([
          new Promise(function(resolve)   { setTimeout(resolve,1000,"foo"); }),
          new Promise(function(_, reject) { setTimeout(reject, 2000,"bar"); }),
        ]));
        var rejects = Promise.race(global.__createIterableObject([
          new Promise(function(_, reject) { setTimeout(reject, 1000,"baz"); }),
          new Promise(function(resolve)   { setTimeout(resolve,2000,"qux"); }),
        ]));
        var score = 0;
        fulfills.then(function(result) { score += (result === "foo"); check(); });
        rejects.catch(function(result) { score += (result === "baz"); check(); });

        function check() {
          if (score === 2) asyncTestPassed();
        }
      
}

if (!test())
    throw new Error("Test failed");
