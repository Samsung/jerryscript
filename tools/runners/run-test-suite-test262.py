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
import re
import subprocess
import sys

import util

def get_platform_cmd_prefix():
    if sys.platform == 'win32':
        return ['cmd', '/S', '/C']
    return ['python2']  # The official test262.py isn't python3 compatible, but has python shebang.


def get_arguments():
    execution_runtime = os.environ.get('RUNTIME', '')
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', metavar='FILE', default=execution_runtime,
                        help='Execution runtime (e.g. qemu)')
    parser.add_argument('--engine', metavar='FILE', required=True,
                        help='JerryScript binary to run tests with')
    parser.add_argument('--test262-object', action='store_true', default=False,
                        help='JerryScript engine create test262 object')
    parser.add_argument('--test-dir', metavar='DIR', required=True,
                        help='Directory contains test262 test suite')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--es5.1', dest='es5_1', default=False, const='default',
                       nargs='?', choices=['default', 'all', 'update'],
                       help='Run test262 - ES5.1. default: all tests except excludelist, ' +
                       'all: all tests, update: all tests and update excludelist')
    group.add_argument('--es2015', default=False, const='default',
                       nargs='?', choices=['default', 'all', 'update'],
                       help='Run test262 - ES2015. default: all tests except excludelist, ' +
                       'all: all tests, update: all tests and update excludelist')
    group.add_argument('--esnext', default=False, const='default',
                       nargs='?', choices=['default', 'all', 'update'],
                       help='Run test262 - ES.next. default: all tests except excludelist, ' +
                       'all: all tests, update: all tests and update excludelist')
    parser.add_argument('--test262-test-list', metavar='LIST',
                        help='Add a comma separated list of tests or directories to run in test262 test suite')

    args = parser.parse_args()

    if args.es2015:
        args.test_dir = os.path.join(args.test_dir, 'es2015')
        args.test262_git_hash = 'fd44cd73dfbce0b515a2474b7cd505d6176a9eb5'
        args.excludelist_path = os.path.join('tests', 'test262-es6-excludelist.xml')
    elif args.esnext:
        args.test_dir = os.path.join(args.test_dir, 'esnext')
        args.test262_git_hash = '281eb10b2844929a7c0ac04527f5b42ce56509fd'
        args.excludelist_path = os.path.join('tests', 'test262-esnext-excludelist.xml')
    else:
        args.test_dir = os.path.join(args.test_dir, 'es5.1')
        args.test262_git_hash = 'es5-tests'
        args.excludelist_path = os.path.join('tests', 'test262-es5.1-excludelist.xml')

    args.mode = args.es2015 or args.esnext or args.es5_1

    return args


def prepare_test262_test_suite(args):
    if os.path.isdir(os.path.join(args.test_dir, '.git')):
        return 0

    return_code = subprocess.call(['git', 'clone', '--no-checkout',
                                   'https://github.com/tc39/test262.git', args.test_dir])
    if return_code:
        print('Cloning test262 repository failed.')
        return return_code

    return_code = subprocess.call(['git', 'checkout', args.test262_git_hash], cwd=args.test_dir)
    assert not return_code, 'Cloning test262 repository failed - invalid git revision.'

    # Since ES2018 iterator's next method is called once during the prologue of iteration,
    # rather than during each step. The test is incorrect and stuck in an infinite loop.
    # https://github.com/tc39/test262/pull/1248 fixed the test and it passes on test262-esnext.
    if args.es2015:
        test_to_remove = 'test/language/statements/for-of/iterator-next-reference.js'
        path_to_remove = os.path.join(args.test_dir, os.path.normpath(test_to_remove))
        if os.path.isfile(path_to_remove):
            os.remove(path_to_remove)

    return 0


def update_exclude_list(args):
    print("=== Summary - updating excludelist ===\n")
    passing_tests = set()
    failing_tests = set()
    new_passing_tests = set()
    with open(os.path.join(os.path.dirname(args.engine), 'test262.report'), 'r') as report_file:
        for line in report_file:
            match = re.match('(=== )?(.*) (?:failed|passed) in (?:non-strict|strict)', line)
            if match:
                (unexpected, test) = match.groups()
                test = test.replace('\\', '/')
                if unexpected:
                    failing_tests.add(test + '.js')
                else:
                    passing_tests.add(test + '.js')

    # Tests pass in strict-mode but fail in non-strict-mode (or vice versa) should be considered as failures
    passing_tests = passing_tests - failing_tests

    with open(args.excludelist_path, 'r+') as exclude_file:
        lines = exclude_file.readlines()
        exclude_file.seek(0)
        exclude_file.truncate()

        # Skip the last line "</excludeList>" to be able to insert new failing tests.
        for line in lines[:-1]:
            match = re.match(r"  <test id=\"(\S*)\">", line)
            if match:
                test = match.group(1)
                if test in failing_tests:
                    failing_tests.remove(test)
                    exclude_file.write(line)
                elif test in passing_tests:
                    new_passing_tests.add(test)
                else:
                    exclude_file.write(line)
            else:
                exclude_file.write(line)

        if failing_tests:
            print("New failing tests added to the excludelist")
            for test in sorted(failing_tests):
                exclude_file.write('  <test id="' + test + '"><reason></reason></test>\n')
                print("  " + test)
            print("")

        exclude_file.write('</excludeList>\n')

    if new_passing_tests:
        print("New passing tests removed from the excludelist")
        for test in sorted(new_passing_tests):
            print("  " + test)
        print("")

    if failing_tests or new_passing_tests:
        print("Excludelist was updated succesfully.")
        return 1

    print("Excludelist was already up-to-date.")
    return 0


def main(args):
    return_code = prepare_test262_test_suite(args)
    if return_code:
        return return_code

    if sys.platform == 'win32':
        original_timezone = util.get_timezone()
        util.set_sighdl_to_reset_timezone(original_timezone)
        util.set_timezone('Pacific Standard Time')

    command = (args.runtime + ' ' + args.engine).strip()
    if args.test262_object:
        command += ' --test262-object'

    kwargs = {}
    if sys.version_info.major >= 3:
        kwargs['errors'] = 'ignore'

    test262_harness_dir = os.path.abspath(os.path.dirname(__file__))
    test262_harness_path = os.path.join(test262_harness_dir, 'test262-harness.py')
    test262_command = get_platform_cmd_prefix() + \
                      [test262_harness_path,
                       '--command', command,
                       '--tests', args.test_dir,
                       '--summary']
    if args.es5_1:
        test262_command.extend(['--es5.1'])
        test262_command.extend(['--unmarked_default', 'non_strict'])

    if 'excludelist_path' in args and args.mode == 'default':
        test262_command.extend(['--exclude-list', args.excludelist_path])

    if args.test262_test_list:
        test262_command.extend(args.test262_test_list.split(','))

    proc = subprocess.Popen(test262_command,
                            universal_newlines=True,
                            stdout=subprocess.PIPE,
                            **kwargs)

    return_code = 1
    with open(os.path.join(os.path.dirname(args.engine), 'test262.report'), 'w') as output_file:
        counter = 0
        summary_found = False
        summary_end_found = False
        while True:
            output = proc.stdout.readline()
            if not output:
                break
            output_file.write(output)

            if output.startswith('=== Summary ==='):
                summary_found = True
                print('')

            if summary_found:
                if not summary_end_found:
                    print(output, end='')
                    if not output.strip():
                        summary_end_found = True
                if 'All tests succeeded' in output:
                    return_code = 0
            elif re.search('in (non-)?strict mode', output):
                counter += 1
                if (counter % 100) == 0:
                    print(".", end='')
                if (counter % 5000) == 0:
                    print(" Executed %d tests." % counter)

    proc.wait()

    if sys.platform == 'win32':
        util.set_timezone(original_timezone)

    if args.mode == 'update':
        return_code = update_exclude_list(args)

    return return_code


if __name__ == "__main__":
    sys.exit(main(get_arguments()))
