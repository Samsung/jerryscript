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
import collections
import hashlib
import os
import platform
import subprocess
import sys
import settings

OUTPUT_DIR = os.path.join(settings.PROJECT_DIR, 'build', 'tests')

Options = collections.namedtuple('Options', ['name', 'build_args', 'test_args', 'skip'])
Options.__new__.__defaults__ = ([], [], False)

def skip_if(condition, desc):
    return desc if condition else False

OPTIONS_PROFILE_MIN = ['--profile=minimal']
OPTIONS_PROFILE_ES51 = [] # NOTE: same as ['--profile=es5.1']
OPTIONS_PROFILE_ES2015 = ['--profile=es2015-subset']
OPTIONS_DEBUG = ['--debug']
OPTIONS_SNAPSHOT = ['--snapshot-save=on', '--snapshot-exec=on', '--jerry-cmdline-snapshot=on']
OPTIONS_UNITTESTS = ['--unittests=on', '--jerry-cmdline=off', '--error-messages=on',
                     '--snapshot-save=on', '--snapshot-exec=on', '--vm-exec-stop=on',
                     '--line-info=on', '--mem-stats=on', '--mem-track-allocation-sizes=on',
                     '--heap-snapshot=on']
OPTIONS_DOCTESTS = ['--doctests=on', '--jerry-cmdline=off', '--error-messages=on',
                    '--snapshot-save=on', '--snapshot-exec=on', '--vm-exec-stop=on',
                    '--heap-snapshot=on']

# Test options for unittests
JERRY_UNITTESTS_OPTIONS = [
    Options('unittests-es2015_subset',
            OPTIONS_UNITTESTS + OPTIONS_PROFILE_ES2015),
    Options('unittests-es2015_subset-debug',
            OPTIONS_UNITTESTS + OPTIONS_PROFILE_ES2015 + OPTIONS_DEBUG),
    Options('doctests-es2015_subset',
            OPTIONS_DOCTESTS + OPTIONS_PROFILE_ES2015),
    Options('doctests-es2015_subset-debug',
            OPTIONS_DOCTESTS + OPTIONS_PROFILE_ES2015 + OPTIONS_DEBUG),
    Options('unittests-es5.1',
            OPTIONS_UNITTESTS + OPTIONS_PROFILE_ES51),
    Options('unittests-es5.1-debug',
            OPTIONS_UNITTESTS + OPTIONS_PROFILE_ES51 + OPTIONS_DEBUG),
    Options('doctests-es5.1',
            OPTIONS_DOCTESTS + OPTIONS_PROFILE_ES51),
    Options('doctests-es5.1-debug',
            OPTIONS_DOCTESTS + OPTIONS_PROFILE_ES51 + OPTIONS_DEBUG)
]

# Test options for jerry-tests
JERRY_TESTS_OPTIONS = [
    Options('jerry_tests-es5.1',
            OPTIONS_PROFILE_ES51),
    Options('jerry_tests-es5.1-snapshot',
            OPTIONS_PROFILE_ES51 + OPTIONS_SNAPSHOT,
            ['--snapshot']),
    Options('jerry_tests-es5.1-debug',
            OPTIONS_PROFILE_ES51 + OPTIONS_DEBUG),
    Options('jerry_tests-es5.1-debug-snapshot',
            OPTIONS_PROFILE_ES51 + OPTIONS_SNAPSHOT + OPTIONS_DEBUG,
            ['--snapshot']),
    Options('jerry_tests-es5.1-debug-cpointer_32bit',
            OPTIONS_PROFILE_ES51 + OPTIONS_DEBUG + ['--cpointer-32bit=on', '--mem-heap=1024']),
    Options('jerry_tests-es5.1-debug-external_context',
            OPTIONS_PROFILE_ES51 + OPTIONS_DEBUG + ['--external-context=on']),
    Options('jerry_tests-es2015_subset-debug',
            OPTIONS_PROFILE_ES2015 + OPTIONS_DEBUG),
]

# Test options for jerry-test-suite
JERRY_TEST_SUITE_OPTIONS = JERRY_TESTS_OPTIONS[:]
JERRY_TEST_SUITE_OPTIONS.extend([
    Options('jerry_test_suite-minimal',
            OPTIONS_PROFILE_MIN),
    Options('jerry_test_suite-minimal-snapshot',
            OPTIONS_PROFILE_MIN + OPTIONS_SNAPSHOT,
            ['--snapshot']),
    Options('jerry_test_suite-minimal-debug',
            OPTIONS_PROFILE_MIN + OPTIONS_DEBUG),
    Options('jerry_test_suite-minimal-debug-snapshot',
            OPTIONS_PROFILE_MIN + OPTIONS_SNAPSHOT + OPTIONS_DEBUG,
            ['--snapshot']),
    Options('jerry_test_suite-es2015_subset',
            OPTIONS_PROFILE_ES2015),
    Options('jerry_test_suite-es2015_subset-snapshot',
            OPTIONS_PROFILE_ES2015 + OPTIONS_SNAPSHOT,
            ['--snapshot']),
    Options('jerry_test_suite-es2015_subset-debug-snapshot',
            OPTIONS_PROFILE_ES2015 + OPTIONS_SNAPSHOT + OPTIONS_DEBUG,
            ['--snapshot']),
])

# Test options for test262
TEST262_TEST_SUITE_OPTIONS = [
    Options('test262_tests')
]

# Test options for jerry-debugger
DEBUGGER_TEST_OPTIONS = [
    Options('jerry_debugger_tests',
            ['--debug', '--jerry-debugger=on'])
]

# Test options for buildoption-test
JERRY_BUILDOPTIONS = [
    Options('buildoption_test-lto',
            ['--lto=on']),
    Options('buildoption_test-error_messages',
            ['--error-messages=on']),
    Options('buildoption_test-logging',
            ['--logging=on']),
    Options('buildoption_test-all_in_one',
            ['--all-in-one=on']),
    Options('buildoption_test-valgrind',
            ['--valgrind=on']),
    Options('buildoption_test-mem_stats',
            ['--mem-stats=on']),
    Options('buildoption_test-show_opcodes',
            ['--show-opcodes=on']),
    Options('buildoption_test-show_regexp_opcodes',
            ['--show-regexp-opcodes=on']),
    Options('buildoption_test-cpointer_32bit',
            ['--compile-flag=-m32', '--cpointer-32bit=on', '--system-allocator=on'],
            skip=skip_if(
                platform.system() != 'Linux' or (platform.machine() != 'i386' and platform.machine() != 'x86_64'),
                '-m32 is only supported on x86[-64]-linux')
           ),
    Options('buildoption_test-no_jerry_libm',
            ['--jerry-libm=off', '--link-lib=m']),
    Options('buildoption_test-no_lcache_prophashmap',
            ['--compile-flag=-DCONFIG_ECMA_LCACHE_DISABLE', '--compile-flag=-DCONFIG_ECMA_PROPERTY_HASHMAP_DISABLE']),
    Options('buildoption_test-external_context',
            ['--external-context=on']),
    Options('buildoption_test-shared_libs',
            ['--shared-libs=on']),
    Options('buildoption_test-cmdline_test',
            ['--jerry-cmdline-test=on']),
    Options('buildoption_test-cmdline_snapshot',
            ['--jerry-cmdline-snapshot=on']),
]

def get_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument('--toolchain', metavar='FILE',
                        help='Add toolchain file')
    parser.add_argument('-q', '--quiet', action='store_true',
                        help='Only print out failing tests')
    parser.add_argument('--buildoptions', metavar='LIST',
                        help='Add a comma separated list of extra build options to each test')
    parser.add_argument('--skip-list', metavar='LIST',
                        help='Add a comma separated list of patterns of the excluded JS-tests')
    parser.add_argument('--outdir', metavar='DIR', default=OUTPUT_DIR,
                        help='Specify output directory (default: %(default)s)')
    parser.add_argument('--check-signed-off', metavar='TYPE', nargs='?',
                        choices=['strict', 'tolerant', 'travis'], const='strict',
                        help='Run signed-off check (%(choices)s; default type if not given: %(const)s)')
    parser.add_argument('--check-cppcheck', action='store_true',
                        help='Run cppcheck')
    parser.add_argument('--check-doxygen', action='store_true',
                        help='Run doxygen')
    parser.add_argument('--check-pylint', action='store_true',
                        help='Run pylint')
    parser.add_argument('--check-vera', action='store_true',
                        help='Run vera check')
    parser.add_argument('--check-license', action='store_true',
                        help='Run license check')
    parser.add_argument('--check-magic-strings', action='store_true',
                        help='Run "magic string source code generator should be executed" check')
    parser.add_argument('--jerry-debugger', action='store_true',
                        help='Run jerry-debugger tests')
    parser.add_argument('--jerry-tests', action='store_true',
                        help='Run jerry-tests')
    parser.add_argument('--jerry-test-suite', action='store_true',
                        help='Run jerry-test-suite')
    parser.add_argument('--test262', action='store_true',
                        help='Run test262')
    parser.add_argument('--unittests', action='store_true',
                        help='Run unittests (including doctests)')
    parser.add_argument('--buildoption-test', action='store_true',
                        help='Run buildoption-test')
    parser.add_argument('--all', '--precommit', action='store_true',
                        help='Run all tests')

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)

    script_args = parser.parse_args()

    return script_args

BINARY_CACHE = {}

TERM_NORMAL = '\033[0m'
TERM_YELLOW = '\033[1;33m'
TERM_BLUE = '\033[1;34m'

def report_command(cmd_type, cmd, env=None):
    sys.stderr.write('%s%s%s\n' % (TERM_BLUE, cmd_type, TERM_NORMAL))
    if env is not None:
        sys.stderr.write(''.join('%s%s=%r \\%s\n' % (TERM_BLUE, var, val, TERM_NORMAL)
                                 for var, val in sorted(env.items())))
    sys.stderr.write('%s%s%s\n' % (TERM_BLUE, (' \\%s\n\t%s' % (TERM_NORMAL, TERM_BLUE)).join(cmd), TERM_NORMAL))

def report_skip(job):
    sys.stderr.write('%sSkipping: %s' % (TERM_YELLOW, job.name))
    if job.skip:
        sys.stderr.write(' (%s)' % job.skip)
    sys.stderr.write('%s\n' % TERM_NORMAL)

def create_binary(job, options):
    build_args = job.build_args[:]
    if options.buildoptions:
        for option in options.buildoptions.split(','):
            if option not in build_args:
                build_args.append(option)

    build_cmd = [settings.BUILD_SCRIPT] + build_args

    build_dir_path = os.path.join(options.outdir, job.name)
    build_cmd.append('--builddir=%s' % build_dir_path)

    install_dir_path = os.path.join(build_dir_path, 'local')
    build_cmd.append('--install=%s' % install_dir_path)

    if options.toolchain:
        build_cmd.append('--toolchain=%s' % options.toolchain)

    report_command('Build command:', build_cmd)

    binary_key = tuple(sorted(build_args))
    if binary_key in BINARY_CACHE:
        ret, build_dir_path = BINARY_CACHE[binary_key]
        sys.stderr.write('(skipping: already built at %s with returncode %d)\n' % (build_dir_path, ret))
        return ret, build_dir_path

    try:
        subprocess.check_output(build_cmd)
        ret = 0
    except subprocess.CalledProcessError as err:
        ret = err.returncode

    BINARY_CACHE[binary_key] = (ret, build_dir_path)
    return ret, build_dir_path

def get_binary_path(build_dir_path):
    return os.path.join(build_dir_path, 'local', 'bin', 'jerry')

def hash_binary(bin_path):
    blocksize = 65536
    hasher = hashlib.sha1()
    with open(bin_path, 'rb') as bin_file:
        buf = bin_file.read(blocksize)
        while len(buf) > 0:
            hasher.update(buf)
            buf = bin_file.read(blocksize)
    return hasher.hexdigest()

def iterate_test_runner_jobs(jobs, options):
    tested_paths = set()
    tested_hashes = {}

    for job in jobs:
        ret_build, build_dir_path = create_binary(job, options)
        if ret_build:
            yield job, ret_build, None

        if build_dir_path in tested_paths:
            sys.stderr.write('(skipping: already tested with %s)\n' % build_dir_path)
            continue
        else:
            tested_paths.add(build_dir_path)

        bin_path = get_binary_path(build_dir_path)
        bin_hash = hash_binary(bin_path)

        if bin_hash in tested_hashes:
            sys.stderr.write('(skipping: already tested with equivalent %s)\n' % tested_hashes[bin_hash])
            continue
        else:
            tested_hashes[bin_hash] = build_dir_path

        test_cmd = [settings.TEST_RUNNER_SCRIPT, bin_path]

        yield job, ret_build, test_cmd

def run_check(runnable, env=None):
    report_command('Test command:', runnable, env=env)

    if env is not None:
        full_env = dict(os.environ)
        full_env.update(env)
        env = full_env

    proc = subprocess.Popen(runnable, env=env)
    proc.wait()
    return proc.returncode

def run_jerry_debugger_tests(options):
    ret_build = ret_test = 0
    for job in DEBUGGER_TEST_OPTIONS:
        ret_build, build_dir_path = create_binary(job, options)
        if ret_build:
            break

        for test_file in os.listdir(settings.DEBUGGER_TESTS_DIR):
            if test_file.endswith(".cmd"):
                test_case, _ = os.path.splitext(test_file)
                test_case_path = os.path.join(settings.DEBUGGER_TESTS_DIR, test_case)
                test_cmd = [
                    settings.DEBUGGER_TEST_RUNNER_SCRIPT,
                    get_binary_path(build_dir_path),
                    settings.DEBUGGER_CLIENT_SCRIPT,
                    os.path.relpath(test_case_path, settings.PROJECT_DIR)
                ]

                if job.test_args:
                    test_cmd.extend(job.test_args)

                ret_test |= run_check(test_cmd)

    return ret_build | ret_test

def run_jerry_tests(options):
    ret_build = ret_test = 0
    for job, ret_build, test_cmd in iterate_test_runner_jobs(JERRY_TESTS_OPTIONS, options):
        if ret_build:
            break

        test_cmd.append(settings.JERRY_TESTS_DIR)

        if options.quiet:
            test_cmd.append("-q")

        skip_list = []

        if '--profile=es2015-subset' in job.build_args:
            skip_list.append(r"es5.1\/")
        else:
            skip_list.append(r"es2015\/")

        if options.skip_list:
            skip_list.append(options.skip_list)

        if skip_list:
            test_cmd.append("--skip-list=" + ",".join(skip_list))

        if job.test_args:
            test_cmd.extend(job.test_args)

        ret_test |= run_check(test_cmd, env=dict(TZ='UTC'))

    return ret_build | ret_test

def run_jerry_test_suite(options):
    ret_build = ret_test = 0
    for job, ret_build, test_cmd in iterate_test_runner_jobs(JERRY_TEST_SUITE_OPTIONS, options):
        if ret_build:
            break

        if '--profile=minimal' in job.build_args:
            test_cmd.append(settings.JERRY_TEST_SUITE_MINIMAL_LIST)
        elif '--profile=es2015-subset' in job.build_args:
            test_cmd.append(settings.JERRY_TEST_SUITE_DIR)
        else:
            test_cmd.append(settings.JERRY_TEST_SUITE_ES51_LIST)

        if options.quiet:
            test_cmd.append("-q")

        if options.skip_list:
            test_cmd.append("--skip-list=" + options.skip_list)

        if job.test_args:
            test_cmd.extend(job.test_args)

        ret_test |= run_check(test_cmd)

    return ret_build | ret_test

def run_test262_test_suite(options):
    ret_build = ret_test = 0
    for job in TEST262_TEST_SUITE_OPTIONS:
        ret_build, build_dir_path = create_binary(job, options)
        if ret_build:
            break

        test_cmd = [
            settings.TEST262_RUNNER_SCRIPT,
            get_binary_path(build_dir_path),
            settings.TEST262_TEST_SUITE_DIR
        ]

        if job.test_args:
            test_cmd.extend(job.test_args)

        ret_test |= run_check(test_cmd, env=dict(TZ='America/Los_Angeles'))

    return ret_build | ret_test

def run_unittests(options):
    ret_build = ret_test = 0
    for job in JERRY_UNITTESTS_OPTIONS:
        ret_build, build_dir_path = create_binary(job, options)
        if ret_build:
            break

        ret_test |= run_check(
            [settings.UNITTEST_RUNNER_SCRIPT] +
            [os.path.join(build_dir_path, 'tests')] +
            (["-q"] if options.quiet else [])
        )

    return ret_build | ret_test

def run_buildoption_test(options):
    for job in JERRY_BUILDOPTIONS:
        if job.skip:
            report_skip(job)
            continue

        ret, _ = create_binary(job, options)
        if ret:
            break

    return ret

Check = collections.namedtuple('Check', ['enabled', 'runner', 'arg'])

def main(options):
    checks = [
        Check(options.check_signed_off, run_check, [settings.SIGNED_OFF_SCRIPT]
              + {'tolerant': ['--tolerant'], 'travis': ['--travis']}.get(options.check_signed_off, [])),
        Check(options.check_cppcheck, run_check, [settings.CPPCHECK_SCRIPT]),
        Check(options.check_doxygen, run_check, [settings.DOXYGEN_SCRIPT]),
        Check(options.check_pylint, run_check, [settings.PYLINT_SCRIPT]),
        Check(options.check_vera, run_check, [settings.VERA_SCRIPT]),
        Check(options.check_license, run_check, [settings.LICENSE_SCRIPT]),
        Check(options.check_magic_strings, run_check, [settings.MAGIC_STRINGS_SCRIPT]),
        Check(options.jerry_debugger, run_jerry_debugger_tests, options),
        Check(options.jerry_tests, run_jerry_tests, options),
        Check(options.jerry_test_suite, run_jerry_test_suite, options),
        Check(options.test262, run_test262_test_suite, options),
        Check(options.unittests, run_unittests, options),
        Check(options.buildoption_test, run_buildoption_test, options),
    ]

    for check in checks:
        if check.enabled or options.all:
            ret = check.runner(check.arg)
            if ret:
                sys.exit(ret)

if __name__ == "__main__":
    main(get_arguments())
