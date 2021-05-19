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
#ifndef _JERRY_MBED_UTIL_LOGGING_H
#define _JERRY_MBED_UTIL_LOGGING_H

#include "mbed.h"

#ifdef LOGPRINT
#define LOG_PRINT(...) printf(__VA_ARGS__)
#else
#define LOG_PRINT(...) while(0) { }
#endif

#define LOG_PRINT_ALWAYS(...) printf(__VA_ARGS__)

#endif // _JERRY_MBED_UTIL_LOGGING_H
