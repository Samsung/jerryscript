#!/usr/bin/env python

# Copyright JS Foundation and other contributors, http://js.foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from os import path

TOOLS_DIR = path.dirname(path.abspath(__file__))
PROJECT_DIR = path.normpath(path.join(TOOLS_DIR, '..'))
DEBUGGER_TESTS_DIR = path.join(PROJECT_DIR, 'tests/debugger')
JERRY_TESTS_DIR = path.join(PROJECT_DIR, 'tests/jerry')
TEST262_TEST_SUITE_DIR = path.join(PROJECT_DIR, 'tests/test262')

BUILD_SCRIPT = path.join(TOOLS_DIR, 'build.py')
CPPCHECK_SCRIPT = path.join(TOOLS_DIR, 'check-cppcheck.sh')
DEBUGGER_CLIENT_SCRIPT = path.join(PROJECT_DIR, 'jerry-debugger/jerry_client.py')
DEBUGGER_TEST_RUNNER_SCRIPT = path.join(TOOLS_DIR, 'runners/run-debugger-test.sh')
DOXYGEN_SCRIPT = path.join(TOOLS_DIR, 'check-doxygen.sh')
LICENSE_SCRIPT = path.join(TOOLS_DIR, 'check-license.py')
MAGIC_STRINGS_SCRIPT = path.join(TOOLS_DIR, 'check-magic-strings.sh')
PYLINT_SCRIPT = path.join(TOOLS_DIR, 'check-pylint.sh')
SIGNED_OFF_SCRIPT = path.join(TOOLS_DIR, 'check-signed-off.sh')
TEST_RUNNER_SCRIPT = path.join(TOOLS_DIR, 'runners/run-test-suite.py')
TEST262_RUNNER_SCRIPT = path.join(TOOLS_DIR, 'runners/run-test-suite-test262.py')
FORMAT_SCRIPT = path.join(TOOLS_DIR, 'check-format.sh')
UNITTEST_RUNNER_SCRIPT = path.join(TOOLS_DIR, 'runners/run-unittests.py')
