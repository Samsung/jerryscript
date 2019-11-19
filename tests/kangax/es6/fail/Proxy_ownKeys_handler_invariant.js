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

function test() {

        var passed = false;
        new Proxy({},{});
        // The Type of each result List element is either String or Symbol.
        try {
          Object.keys(new Proxy({}, {
            ownKeys: function () {
              passed = true;
              return [2];
            }}));
          return false;
        } catch(e) {}
        // The result List must contain the keys of all non-configurable own properties of the target object.
        var proxied = {};
        Object.defineProperty(proxied, "foo", { value: 2, writable: true, enumerable: true });
        try {
          Object.keys(new Proxy(proxied, {
            ownKeys: function () {
              return [];
            }}));
          return false;
        } catch(e) {}
        // If the target object is not extensible, then the result List must contain all the keys
        // of the own properties of the target object and no other values.
        try {
          Object.keys(new Proxy(Object.preventExtensions({b:1}), {
            ownKeys: function () {
              return ['a'];
            }}));
          return false;
        } catch(e) {}
        return passed;
      
}

if (!test())
    throw new Error("Test failed");
