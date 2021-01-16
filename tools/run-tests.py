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

if sys.version_info.major >= 3:
    import runners.util as util  # pylint: disable=import-error
else:
    sys.path.append(os.path.dirname(os.path.realpath(__file__)) + '/runners')
    import util

OUTPUT_DIR = os.path.join(settings.PROJECT_DIR, 'build', 'tests')

Options = collections.namedtuple('Options', ['name', 'build_args', 'test_args', 'skip'])
Options.__new__.__defaults__ = ([], [], False)

def skip_if(condition, desc):
    return desc if condition else False

OPTIONS_COMMON = ['--lto=off']
OPTIONS_PROFILE_MIN = ['--profile=minimal']
OPTIONS_PROFILE_ES51 = ['--profile=es5.1']
OPTIONS_PROFILE_ESNEXT = ['--profile=es.next']
OPTIONS_STACK_LIMIT = ['--stack-limit=96']
OPTIONS_GC_MARK_LIMIT = ['--gc-mark-limit=16']
OPTIONS_MEM_STRESS = ['--mem-stress-test=on']
OPTIONS_DEBUG = ['--debug']
OPTIONS_SNAPSHOT = ['--snapshot-save=on', '--snapshot-exec=on', '--jerry-cmdline-snapshot=on']
OPTIONS_UNITTESTS = ['--unittests=on', '--jerry-cmdline=off', '--error-messages=on',
                     '--snapshot-save=on', '--snapshot-exec=on', '--vm-exec-stop=on',
                     '--line-info=on', '--mem-stats=on']
OPTIONS_DOCTESTS = ['--doctests=on', '--jerry-cmdline=off', '--error-messages=on',
                    '--snapshot-save=on', '--snapshot-exec=on', '--vm-exec-stop=on']

# Test options for unittests
JERRY_UNITTESTS_OPTIONS = [
    Options('unittests-es.next',
            OPTIONS_COMMON + OPTIONS_UNITTESTS + OPTIONS_PROFILE_ESNEXT),
    Options('doctests-es.next',
            OPTIONS_COMMON + OPTIONS_DOCTESTS + OPTIONS_PROFILE_ESNEXT),
    Options('unittests-es5.1',
            OPTIONS_COMMON + OPTIONS_UNITTESTS + OPTIONS_PROFILE_ES51),
    Options('doctests-es5.1',
            OPTIONS_COMMON + OPTIONS_DOCTESTS + OPTIONS_PROFILE_ES51),
    Options('unittests-es5.1-init-fini',
            OPTIONS_COMMON + OPTIONS_UNITTESTS + OPTIONS_PROFILE_ES51
            + ['--cmake-param=-DFEATURE_INIT_FINI=ON']),
    Options('unittests-es5.1-math',
            OPTIONS_COMMON + OPTIONS_UNITTESTS + OPTIONS_PROFILE_ES51
            + ['--jerry-math=on']),
]

# Test options for jerry-tests
JERRY_TESTS_OPTIONS = [
    Options('jerry_tests-es.next',
            OPTIONS_COMMON + OPTIONS_PROFILE_ESNEXT + OPTIONS_STACK_LIMIT + OPTIONS_GC_MARK_LIMIT
            + OPTIONS_MEM_STRESS),
    Options('jerry_tests-es5.1',
            OPTIONS_COMMON + OPTIONS_PROFILE_ES51 + OPTIONS_STACK_LIMIT + OPTIONS_GC_MARK_LIMIT),
    Options('jerry_tests-es5.1-snapshot',
            OPTIONS_COMMON + OPTIONS_PROFILE_ES51 + OPTIONS_SNAPSHOT + OPTIONS_STACK_LIMIT + OPTIONS_GC_MARK_LIMIT,
            ['--snapshot']),
    Options('jerry_tests-es5.1-cpointer_32bit',
            OPTIONS_COMMON + OPTIONS_PROFILE_ES51 + OPTIONS_STACK_LIMIT + OPTIONS_GC_MARK_LIMIT
            + ['--cpointer-32bit=on', '--mem-heap=1024']),
    Options('jerry_tests-es5.1-external_context',
            OPTIONS_COMMON + OPTIONS_PROFILE_ES51 + OPTIONS_STACK_LIMIT + OPTIONS_GC_MARK_LIMIT
            + ['--external-context=on']),
]

# Test options for test262
TEST262_TEST_SUITE_OPTIONS = [
    Options('test262_tests', OPTIONS_PROFILE_ES51),
]

# Test options for test262-es2015
TEST262_ES2015_TEST_SUITE_OPTIONS = [
    Options('test262_tests_es2015', OPTIONS_PROFILE_ESNEXT + ['--line-info=on', '--error-messages=on']),
]

# Test options for test262-esnext
TEST262_ESNEXT_TEST_SUITE_OPTIONS = [
    Options('test262_tests_esnext', OPTIONS_PROFILE_ESNEXT
            + ['--line-info=on', '--error-messages=on', '--mem-heap=20480']),
]

# Test options for jerry-debugger
DEBUGGER_TEST_OPTIONS = [
    Options('jerry_debugger_tests',
            ['--jerry-debugger=on'])
]

# Test options for buildoption-test
JERRY_BUILDOPTIONS = [
    Options('buildoption_test-lto',
            ['--lto=on']),
    Options('buildoption_test-error_messages',
            ['--error-messages=on']),
    Options('buildoption_test-logging',
            ['--logging=on']),
    Options('buildoption_test-amalgam',
            ['--amalgam=on']),
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
    Options('buildoption_test-jerry_math',
            ['--jerry-math=on']),
    Options('buildoption_test-no_lcache_prophashmap',
            ['--compile-flag=-DJERRY_LCACHE=0', '--compile-flag=-DJERRY_PROPRETY_HASHMAP=0']),
    Options('buildoption_test-external_context',
            ['--external-context=on']),
    Options('buildoption_test-shared_libs',
            ['--shared-libs=on'],
            skip=skip_if((sys.platform == 'win32'), 'Not yet supported, link failure on Windows')),
    Options('buildoption_test-cmdline_test',
            ['--jerry-cmdline-test=on'],
            skip=skip_if((sys.platform == 'win32'), 'rand() can\'t be overriden on Windows (benchmarking.c)')),
    Options('buildoption_test-cmdline_snapshot',
            ['--jerry-cmdline-snapshot=on']),
    Options('buildoption_test-recursion_limit',
            OPTIONS_STACK_LIMIT),
    Options('buildoption_test-gc-mark_limit',
            OPTIONS_GC_MARK_LIMIT),
    Options('buildoption_test-jerry-debugger',
            ['--jerry-debugger=on']),
    Options('buildoption_test-module-off',
            ['--compile-flag=-DJERRY_MODULE_SYSTEM=0', '--lto=off']),
    Options('buildoption_test-builtin-proxy-off',
            ['--compile-flag=-DJERRY_BUILTIN_PROXY=0']),
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
                        choices=['strict', 'tolerant', 'gh-actions'], const='strict',
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
    parser.add_argument('--build-debug', action='store_true',
                        help='Build debug version jerryscript')
    parser.add_argument('--jerry-debugger', action='store_true',
                        help='Run jerry-debugger tests')
    parser.add_argument('--jerry-tests', action='store_true',
                        help='Run jerry-tests')
    parser.add_argument('--test262', action='store_true',
                        help='Run test262 - ES5.1')
    parser.add_argument('--test262-es2015', default=False, const='default',
                        nargs='?', choices=['default', 'all', 'update'],
                        help='Run test262 - ES2015. default: all tests except excludelist, ' +
                        'all: all tests, update: all tests and update excludelist')
    parser.add_argument('--test262-esnext', default=False, const='default',
                        nargs='?', choices=['default', 'all', 'update'],
                        help='Run test262 - ESnext. default: all tests except excludelist, ' +
                        'all: all tests, update: all tests and update excludelist')
    parser.add_argument('--test262-test-list', metavar='LIST',
                        help='Add a comma separated list of tests or directories to run in test262 test suite')
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

    if script_args.test262_test_list and not \
       (script_args.test262 or script_args.test262_es2015 or script_args.test262_esnext):
        print("--test262-test-list is only allowed with --test262 or --test262-es2015 or --test262-esnext\n")
        parser.print_help()
        sys.exit(1)

    return script_args

BINARY_CACHE = {}

TERM_NORMAL = '\033[0m'
TERM_YELLOW = '\033[1;33m'
TERM_BLUE = '\033[1;34m'
TERM_RED = '\033[1;31m'

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
    build_dir_path = os.path.join(options.outdir, job.name)
    if options.build_debug:
        build_args.extend(OPTIONS_DEBUG)
        build_dir_path = os.path.join(options.outdir, job.name + '-debug')
    if options.buildoptions:
        for option in options.buildoptions.split(','):
            if option not in build_args:
                build_args.append(option)

    build_cmd = util.get_python_cmd_prefix()
    build_cmd.append(settings.BUILD_SCRIPT)
    build_cmd.extend(build_args)

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
        print(err.output)
        ret = err.returncode

    BINARY_CACHE[binary_key] = (ret, build_dir_path)
    return ret, build_dir_path

def get_binary_path(build_dir_path):
    executable_extension = '.exe' if sys.platform == 'win32' else ''
    return os.path.join(build_dir_path, 'local', 'bin', 'jerry' + executable_extension)

def hash_binary(bin_path):
    blocksize = 65536
    hasher = hashlib.sha1()
    with open(bin_path, 'rb') as bin_file:
        buf = bin_file.read(blocksize)
        while buf:
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

        test_cmd = util.get_python_cmd_prefix()
        test_cmd.extend([settings.TEST_RUNNER_SCRIPT, '--engine', bin_path])

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
            print("\n%sBuild failed%s\n" % (TERM_RED, TERM_NORMAL))
            break

        for channel in ["websocket", "rawpacket"]:
            for test_file in os.listdir(settings.DEBUGGER_TESTS_DIR):
                if test_file.endswith(".cmd"):
                    test_case, _ = os.path.splitext(test_file)
                    test_case_path = os.path.join(settings.DEBUGGER_TESTS_DIR, test_case)
                    test_cmd = [
                        settings.DEBUGGER_TEST_RUNNER_SCRIPT,
                        get_binary_path(build_dir_path),
                        channel,
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

        test_cmd.append('--test-dir')
        test_cmd.append(settings.JERRY_TESTS_DIR)

        if options.quiet:
            test_cmd.append("-q")

        skip_list = []

        if '--profile=es.next' in job.build_args:
            skip_list.append(os.path.join('es5.1', ''))
        else:
            skip_list.append(os.path.join('es.next', ''))

        if options.skip_list:
            skip_list.append(options.skip_list)

        if skip_list:
            test_cmd.append("--skip-list=" + ",".join(skip_list))

        if job.test_args:
            test_cmd.extend(job.test_args)

        ret_test |= run_check(test_cmd, env=dict(TZ='UTC'))

    return ret_build | ret_test

def run_test262_test_suite(options):
    ret_build = ret_test = 0

    jobs = []
    if options.test262:
        jobs.extend(TEST262_TEST_SUITE_OPTIONS)
    if options.test262_es2015:
        jobs.extend(TEST262_ES2015_TEST_SUITE_OPTIONS)
    if options.test262_esnext:
        jobs.extend(TEST262_ESNEXT_TEST_SUITE_OPTIONS)

    for job in jobs:
        ret_build, build_dir_path = create_binary(job, options)
        if ret_build:
            print("\n%sBuild failed%s\n" % (TERM_RED, TERM_NORMAL))
            break

        test_cmd = util.get_python_cmd_prefix() + [
            settings.TEST262_RUNNER_SCRIPT,
            '--engine', get_binary_path(build_dir_path) + " --test262-object",
            '--test-dir', settings.TEST262_TEST_SUITE_DIR
        ]

        if job.name.endswith('es2015'):
            test_cmd.append('--es2015')
            test_cmd.append(options.test262_es2015)
        elif job.name.endswith('esnext'):
            test_cmd.append('--esnext')
            test_cmd.append(options.test262_esnext)
        else:
            test_cmd.append('--es51')

        if job.test_args:
            test_cmd.extend(job.test_args)

        if options.test262_test_list:
            test_cmd.append('--test262-test-list')
            test_cmd.append(options.test262_test_list)

        ret_test |= run_check(test_cmd, env=dict(TZ='America/Los_Angeles'))

    return ret_build | ret_test

def run_unittests(options):
    ret_build = ret_test = 0
    for job in JERRY_UNITTESTS_OPTIONS:
        if job.skip:
            report_skip(job)
            continue
        ret_build, build_dir_path = create_binary(job, options)
        if ret_build:
            print("\n%sBuild failed%s\n" % (TERM_RED, TERM_NORMAL))
            break

        if sys.platform == 'win32':
            if options.build_debug:
                build_config = "Debug"
            else:
                build_config = "MinSizeRel"
        else:
            build_config = ""


        ret_test |= run_check(
            util.get_python_cmd_prefix() +
            [settings.UNITTEST_RUNNER_SCRIPT] +
            [os.path.join(build_dir_path, 'tests', build_config)] +
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
            print("\n%sBuild failed%s\n" % (TERM_RED, TERM_NORMAL))
            break

    return ret

Check = collections.namedtuple('Check', ['enabled', 'runner', 'arg'])

def main(options):
    checks = [
        Check(options.check_signed_off, run_check, [settings.SIGNED_OFF_SCRIPT]
              + {'tolerant': ['--tolerant'], 'gh-actions': ['--gh-actions']}.get(options.check_signed_off, [])),
        Check(options.check_cppcheck, run_check, [settings.CPPCHECK_SCRIPT]),
        Check(options.check_doxygen, run_check, [settings.DOXYGEN_SCRIPT]),
        Check(options.check_pylint, run_check, [settings.PYLINT_SCRIPT]),
        Check(options.check_vera, run_check, [settings.VERA_SCRIPT]),
        Check(options.check_license, run_check, [settings.LICENSE_SCRIPT]),
        Check(options.check_magic_strings, run_check, [settings.MAGIC_STRINGS_SCRIPT]),
        Check(options.jerry_debugger, run_jerry_debugger_tests, options),
        Check(options.jerry_tests, run_jerry_tests, options),
        Check(options.test262 or options.test262_es2015 or options.test262_esnext, run_test262_test_suite, options),
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
