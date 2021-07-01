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

from __future__ import print_function
import argparse
import glob
import os
import subprocess
import sys

import util


def get_arguments():
    runtime = os.environ.get('RUNTIME')
    parser = argparse.ArgumentParser()
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Only print out failing tests')
    parser.add_argument('--runtime', metavar='FILE', default=runtime,
                        help='Execution runtime (e.g. qemu)')
    parser.add_argument('path',
                        help='Path of test binaries')

    script_args = parser.parse_args()
    return script_args


def get_unittests(path):
    unittests = []
    files = glob.glob(os.path.join(path, 'unit-*'))
    for testfile in files:
        if os.path.isfile(testfile) and os.access(testfile, os.X_OK):
            if sys.platform != 'win32' or testfile.endswith(".exe"):
                unittests.append(testfile)
    unittests.sort()
    return unittests


def main(args):
    util.setup_stdio()
    unittests = get_unittests(args.path)
    total = len(unittests)
    if total == 0:
        print("%s: no unit-* test to execute", args.path)
        return 1

    test_cmd = [args.runtime] if args.runtime else []

    tested = 0
    passed = 0
    failed = 0
    for test in unittests:
        tested += 1
        test_path = os.path.relpath(test)
        try:
            subprocess.check_output(test_cmd + [test], stderr=subprocess.STDOUT, universal_newlines=True)
            passed += 1
            if not args.quiet:
                util.print_test_result(tested, total, True, 'PASS', test_path)
        except subprocess.CalledProcessError as err:
            failed += 1
            util.print_test_result(tested, total, False, 'FAIL (%d)' % err.returncode, test_path)
            print("================================================")
            print(err.output)
            print("================================================")

    util.print_test_summary(os.path.join(os.path.relpath(args.path), "unit-*"), total, passed, failed)

    if failed > 0:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(get_arguments()))
