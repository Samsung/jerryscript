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
import os
import subprocess
import sys

import util

def get_arguments():
    execution_runtime = os.environ.get('RUNTIME')
    parser = argparse.ArgumentParser()
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Only print out failing tests')
    parser.add_argument('--runtime', metavar='FILE', default=execution_runtime,
                        help='Execution runtime (e.g. qemu)')
    parser.add_argument('--engine', metavar='FILE',
                        help='JerryScript binary to run tests with')
    parser.add_argument('--test-list', metavar='FILE',
                        help='File contains test paths to run')
    parser.add_argument('--skip-list', metavar='LIST',
                        help='Add a comma separated list of patterns of the excluded JS-tests')
    parser.add_argument('--test-dir', metavar='DIR',
                        help='Directory contains tests to run')
    parser.add_argument('--snapshot', action='store_true',
                        help='Snapshot test')

    script_args = parser.parse_args()
    if script_args.skip_list:
        script_args.skip_list = script_args.skip_list.split(',')
    else:
        script_args.skip_list = []

    return script_args


def get_tests(test_dir, test_list, skip_list):
    tests = []
    if test_dir:
        tests = []
        for root, _, files in os.walk(test_dir):
            for test_file in files:
                if test_file.endswith('.js') or test_file.endswith('.mjs'):
                    tests.extend([os.path.join(root, test_file)])

    if test_list:
        dirname = os.path.dirname(test_list)
        with open(test_list, "r") as test_list_fd:
            for test in test_list_fd:
                tests.append(os.path.normpath(os.path.join(dirname, test.rstrip())))

    tests.sort()

    def filter_tests(test):
        for skipped in skip_list:
            if skipped in test:
                return False
        return True

    return [test for test in tests if filter_tests(test)]


def execute_test_command(test_cmd):
    kwargs = {}
    if sys.version_info.major >= 3:
        kwargs['encoding'] = 'unicode_escape'
        kwargs['errors'] = 'ignore'
    process = subprocess.Popen(test_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               universal_newlines=True, **kwargs)
    stdout = process.communicate()[0]
    return (process.returncode, stdout)


def main(args):
    util.setup_stdio()
    tests = get_tests(args.test_dir, args.test_list, args.skip_list)
    total = len(tests)
    if total == 0:
        print("No test to execute.")
        return 1

    if sys.platform == 'win32':
        original_timezone = util.get_timezone()
        util.set_sighdl_to_reset_timezone(original_timezone)
        util.set_timezone('UTC')

    if args.snapshot:
        passed = run_snapshot_tests(args, tests)
    else:
        passed = run_normal_tests(args, tests)

    if sys.platform == 'win32':
        util.set_timezone(original_timezone)

    failed = total - passed

    summary_list = [os.path.relpath(args.engine)]
    if args.snapshot:
        summary_list.append('--snapshot')
    if args.test_dir:
        summary_list.append(os.path.relpath(args.test_dir))
    if args.test_list:
        summary_list.append(os.path.relpath(args.test_list))
    util.print_test_summary(' '.join(summary_list), total, passed, failed)

    return bool(failed)


def run_normal_tests(args, tests):
    test_cmd = util.get_platform_cmd_prefix()
    if args.runtime:
        test_cmd.append(args.runtime)
    test_cmd.extend([args.engine, '--call-on-exit', '__checkAsync'])

    total = len(tests)
    tested = 0
    passed = 0
    for test in tests:
        tested += 1
        test_path = os.path.relpath(test)
        is_expected_to_fail = os.path.join(os.path.sep, 'fail', '') in test

        test_argument = []
        if test.endswith('.mjs'):
            test_argument.extend(['-m'])

        (returncode, stdout) = execute_test_command(test_cmd + test_argument + [test])

        if (returncode == 0 and not is_expected_to_fail) or (returncode == 1 and is_expected_to_fail):
            passed += 1
            if not args.quiet:
                passed_string = 'PASS' + (' (XFAIL)' if is_expected_to_fail else '')
                util.print_test_result(tested, total, True, passed_string, test_path)
        else:
            passed_string = 'FAIL%s (%d)' % (' (XPASS)' if returncode == 0 and is_expected_to_fail else '', returncode)
            util.print_test_result(tested, total, False, passed_string, test_path)
            print("================================================")
            print(stdout)
            print("================================================")

    return passed


def run_snapshot_tests(args, tests):
    execute_snapshot_cmd = util.get_platform_cmd_prefix()
    generate_snapshot_cmd = util.get_platform_cmd_prefix()
    if args.runtime:
        execute_snapshot_cmd.append(args.runtime)
        generate_snapshot_cmd.append(args.runtime)

    execute_snapshot_cmd.extend([args.engine, '--exec-snapshot', 'js.snapshot'])
    execute_snapshot_cmd.extend(['--call-on-exit', '__checkAsync'])

    # engine: jerry[.exe] -> snapshot generator: jerry-snapshot[.exe]
    engine = os.path.splitext(args.engine)
    generate_snapshot_cmd.append(engine[0] + '-snapshot' + engine[1])
    generate_snapshot_cmd.append('generate')

    total = len(tests)
    tested = 0
    passed = 0
    for test in tests:
        tested += 1
        test_path = os.path.relpath(test)
        is_expected_to_fail = os.path.join(os.path.sep, 'fail', '') in test
        (returncode, stdout) = execute_test_command(generate_snapshot_cmd + [test])

        if (returncode == 0) or (returncode == 1 and is_expected_to_fail):
            if not args.quiet:
                passed_string = 'PASS' + (' (XFAIL)' if returncode else '')
                util.print_test_result(tested, total, True, passed_string, test_path, True)
        else:
            util.print_test_result(tested, total, False, 'FAIL (%d)' % (returncode), test_path, True)
            print("================================================")
            print(stdout)
            print("================================================")

        if returncode:
            if is_expected_to_fail:
                passed += 1
            continue

        (returncode, stdout) = execute_test_command(execute_snapshot_cmd)
        os.remove('js.snapshot')

        if (returncode == 0 and not is_expected_to_fail) or (returncode == 1 and is_expected_to_fail):
            passed += 1
            if not args.quiet:
                passed_string = 'PASS' + (' (XFAIL)' if is_expected_to_fail else '')
                util.print_test_result(tested, total, True, passed_string, test_path, False)
        else:
            passed_string = 'FAIL%s (%d)' % (' (XPASS)' if returncode == 0 and is_expected_to_fail else '', returncode)
            util.print_test_result(tested, total, False, passed_string, test_path, False)
            print("================================================")
            print(stdout)
            print("================================================")

    return passed


if __name__ == "__main__":
    sys.exit(main(get_arguments()))
