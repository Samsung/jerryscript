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
import multiprocessing
import os
import shutil
import subprocess
import sys
import settings

def default_toolchain():
    # We don't have default toolchain on Windows and os.uname() isn't supported.
    if sys.platform == 'win32':
        return None

    (sysname, _, _, _, machine) = os.uname()
    toolchain = os.path.join(settings.PROJECT_DIR,
                             'cmake',
                             'toolchain_%s_%s.cmake' % (sysname.lower(), machine.lower()))
    return toolchain if os.path.isfile(toolchain) else None

def get_arguments():
    devhelp_preparser = argparse.ArgumentParser(add_help=False)
    devhelp_preparser.add_argument('--devhelp', action='store_true', default=False,
                                   help='show help with all options '
                                        '(including those, which are useful for developers only)')

    devhelp_arguments, args = devhelp_preparser.parse_known_args()
    if devhelp_arguments.devhelp:
        args.append('--devhelp')

    def devhelp(helpstring):
        return helpstring if devhelp_arguments.devhelp else argparse.SUPPRESS

    parser = argparse.ArgumentParser(parents=[devhelp_preparser], epilog="""
        This tool is a thin wrapper around cmake and make to help build the
        project easily. All the real build logic is in the CMakeLists.txt files.
        For most of the options, the defaults are also defined there.
        """)

    buildgrp = parser.add_argument_group('general build options')
    buildgrp.add_argument('--builddir', metavar='DIR', default=os.path.join(settings.PROJECT_DIR, 'build'),
                          help='specify build directory (default: %(default)s)')
    buildgrp.add_argument('--clean', action='store_true', default=False,
                          help='clean build')
    buildgrp.add_argument('--cmake-param', metavar='OPT', action='append', default=[],
                          help='add custom argument to CMake')
    buildgrp.add_argument('--compile-flag', metavar='OPT', action='append', default=[],
                          help='add custom compile flag')
    buildgrp.add_argument('--build-type', metavar='TYPE', default='MinSizeRel',
                          help='set build type (default: %(default)s)')
    buildgrp.add_argument('--debug', dest='build_type', action='store_const', const='Debug', default=argparse.SUPPRESS,
                          help='debug build (alias for --build-type %(const)s)')
    buildgrp.add_argument('--install', metavar='DIR', nargs='?', default=None, const=False,
                          help='install after build (default: don\'t install; '
                               'default directory if install: OS-specific)')
    buildgrp.add_argument('-j', '--jobs', metavar='N', type=int, default=multiprocessing.cpu_count() + 1,
                          help='number of parallel build jobs (default: %(default)s)')
    buildgrp.add_argument('--link-lib', metavar='OPT', action='append', default=[],
                          help='add custom library to be linked')
    buildgrp.add_argument('--linker-flag', metavar='OPT', action='append', default=[],
                          help='add custom linker flag')
    buildgrp.add_argument('--amalgam', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                          help='enable amalgamated build (%(choices)s)')
    buildgrp.add_argument('--lto', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                          help='enable link-time optimizations (%(choices)s)')
    buildgrp.add_argument('--shared-libs', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                          help='enable building of shared libraries (%(choices)s)')
    buildgrp.add_argument('--strip', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                          help='strip release binaries (%(choices)s)')
    buildgrp.add_argument('--toolchain', metavar='FILE', default=default_toolchain(),
                          help='specify toolchain file (default: %(default)s)')
    buildgrp.add_argument('-v', '--verbose', action='store_const', const='ON',
                          help='increase verbosity')

    compgrp = parser.add_argument_group('optional components')
    compgrp.add_argument('--doctests', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('build doctests (%(choices)s)'))
    compgrp.add_argument('--jerry-cmdline', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='build jerry command line tool (%(choices)s)')
    compgrp.add_argument('--jerry-cmdline-snapshot', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='build snapshot command line tool (%(choices)s)')
    compgrp.add_argument('--jerry-cmdline-test', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('build test version of the jerry command line tool (%(choices)s)'))
    compgrp.add_argument('--libfuzzer', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('build jerry with libfuzzer support (%(choices)s)'))
    compgrp.add_argument('--oss-fuzz', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('build jerry to run on OSS-fuzz (%(choices)s)'))
    compgrp.add_argument('--jerry-ext', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='build jerry-ext (%(choices)s)')
    compgrp.add_argument('--jerry-math', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='build and use jerry-math (%(choices)s)')
    compgrp.add_argument('--jerry-port-default', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='build default jerry port implementation (%(choices)s)')
    compgrp.add_argument('--unittests', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('build unittests (%(choices)s)'))

    coregrp = parser.add_argument_group('jerry-core options')
    coregrp.add_argument('--cpointer-32bit', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable 32 bit compressed pointers (%(choices)s)')
    coregrp.add_argument('--error-messages', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable error messages (%(choices)s)')
    coregrp.add_argument('--external-context', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable external context (%(choices)s)')
    coregrp.add_argument('--jerry-debugger', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable the jerry debugger (%(choices)s)')
    coregrp.add_argument('--js-parser', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable js-parser (%(choices)s)')
    coregrp.add_argument('--line-info', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='provide line info (%(choices)s)')
    coregrp.add_argument('--logging', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable logging (%(choices)s)')
    coregrp.add_argument('--mem-heap', metavar='SIZE', type=int,
                         help='size of memory heap (in kilobytes)')
    coregrp.add_argument('--gc-limit', metavar='SIZE', type=int,
                         help='memory usage limit to trigger garbage collection (in bytes)')
    coregrp.add_argument('--stack-limit', metavar='SIZE', type=int,
                         help='maximum stack usage (in kilobytes)')
    coregrp.add_argument('--gc-mark-limit', metavar='SIZE', type=int,
                         help='maximum depth of recursion during GC mark phase')
    coregrp.add_argument('--mem-stats', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('enable memory statistics (%(choices)s)'))
    coregrp.add_argument('--mem-stress-test', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('enable mem-stress test (%(choices)s)'))
    coregrp.add_argument('--profile', metavar='FILE',
                         help='specify profile file')
    coregrp.add_argument('--promise-callback', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable promise callback (%(choices)s)')
    coregrp.add_argument('--regexp-strict-mode', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('enable regexp strict mode (%(choices)s)'))
    coregrp.add_argument('--show-opcodes', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('enable parser byte-code dumps (%(choices)s)'))
    coregrp.add_argument('--show-regexp-opcodes', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('enable regexp byte-code dumps (%(choices)s)'))
    coregrp.add_argument('--snapshot-exec', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable executing snapshot files (%(choices)s)')
    coregrp.add_argument('--snapshot-save', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable saving snapshot files (%(choices)s)')
    coregrp.add_argument('--system-allocator', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable system allocator (%(choices)s)')
    coregrp.add_argument('--valgrind', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('enable Valgrind support (%(choices)s)'))
    coregrp.add_argument('--vm-exec-stop', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help='enable VM execution stopping (%(choices)s)')

    maingrp = parser.add_argument_group('jerry-main options')
    maingrp.add_argument('--link-map', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('enable the generation of link map for jerry command line tool (%(choices)s)'))
    maingrp.add_argument('--compile-commands', metavar='X', choices=['ON', 'OFF'], type=str.upper,
                         help=devhelp('enable the generation of compile_commands.json (%(choices)s)'))

    arguments = parser.parse_args(args)
    if arguments.devhelp:
        parser.print_help()
        sys.exit(0)

    return arguments

def generate_build_options(arguments):
    build_options = []

    def build_options_append(cmakeopt, cliarg):
        if cliarg:
            build_options.append('-D%s=%s' % (cmakeopt, cliarg))

    # general build options
    build_options_append('CMAKE_BUILD_TYPE', arguments.build_type)
    build_options_append('EXTERNAL_COMPILE_FLAGS', ' '.join(arguments.compile_flag))
    build_options_append('EXTERNAL_LINK_LIBS', ' '.join(arguments.link_lib))
    build_options_append('EXTERNAL_LINKER_FLAGS', ' '.join(arguments.linker_flag))
    build_options_append('ENABLE_AMALGAM', arguments.amalgam)
    build_options_append('ENABLE_LTO', arguments.lto)
    build_options_append('BUILD_SHARED_LIBS', arguments.shared_libs)
    build_options_append('ENABLE_STRIP', arguments.strip)
    build_options_append('CMAKE_TOOLCHAIN_FILE', arguments.toolchain)
    build_options_append('CMAKE_VERBOSE_MAKEFILE', arguments.verbose)

    # optional components
    build_options_append('DOCTESTS', arguments.doctests)
    build_options_append('JERRY_CMDLINE', arguments.jerry_cmdline)
    build_options_append('JERRY_CMDLINE_SNAPSHOT', arguments.jerry_cmdline_snapshot)
    build_options_append('JERRY_CMDLINE_TEST', arguments.jerry_cmdline_test)
    build_options_append('JERRY_LIBFUZZER', arguments.libfuzzer)
    build_options_append('JERRY_OSS_FUZZ', arguments.oss_fuzz)
    build_options_append('JERRY_EXT', arguments.jerry_ext)
    build_options_append('JERRY_MATH', arguments.jerry_math)
    build_options_append('JERRY_PORT_DEFAULT', arguments.jerry_port_default)
    build_options_append('UNITTESTS', arguments.unittests)

    # jerry-core options
    build_options_append('JERRY_CPOINTER_32_BIT', arguments.cpointer_32bit)
    build_options_append('JERRY_ERROR_MESSAGES', arguments.error_messages)
    build_options_append('JERRY_EXTERNAL_CONTEXT', arguments.external_context)
    build_options_append('JERRY_DEBUGGER', arguments.jerry_debugger)
    build_options_append('JERRY_PARSER', arguments.js_parser)
    build_options_append('JERRY_LINE_INFO', arguments.line_info)
    build_options_append('JERRY_LOGGING', arguments.logging)
    build_options_append('JERRY_GLOBAL_HEAP_SIZE', arguments.mem_heap)
    build_options_append('JERRY_GC_LIMIT', arguments.gc_limit)
    build_options_append('JERRY_STACK_LIMIT', arguments.stack_limit)
    build_options_append('JERRY_MEM_STATS', arguments.mem_stats)
    build_options_append('JERRY_MEM_GC_BEFORE_EACH_ALLOC', arguments.mem_stress_test)
    build_options_append('JERRY_PROFILE', arguments.profile)
    build_options_append('JERRY_PROMISE_CALLBACK', arguments.promise_callback)
    build_options_append('JERRY_REGEXP_STRICT_MODE', arguments.regexp_strict_mode)
    build_options_append('JERRY_PARSER_DUMP_BYTE_CODE', arguments.show_opcodes)
    build_options_append('JERRY_REGEXP_DUMP_BYTE_CODE', arguments.show_regexp_opcodes)
    build_options_append('JERRY_SNAPSHOT_EXEC', arguments.snapshot_exec)
    build_options_append('JERRY_SNAPSHOT_SAVE', arguments.snapshot_save)
    build_options_append('JERRY_SYSTEM_ALLOCATOR', arguments.system_allocator)
    build_options_append('JERRY_VALGRIND', arguments.valgrind)
    build_options_append('JERRY_VM_EXEC_STOP', arguments.vm_exec_stop)

    if arguments.gc_mark_limit is not None:
        build_options.append('-D%s=%s' % ('JERRY_GC_MARK_LIMIT', arguments.gc_mark_limit))

    # jerry-main options
    build_options_append('ENABLE_LINK_MAP', arguments.link_map)
    build_options_append('ENABLE_COMPILE_COMMANDS', arguments.compile_commands)

    # general build options (final step)
    if arguments.cmake_param:
        build_options.extend(arguments.cmake_param)

    return build_options

def configure_output_dir(arguments):
    if not os.path.isabs(arguments.builddir):
        arguments.builddir = os.path.join(settings.PROJECT_DIR, arguments.builddir)

    if arguments.clean and os.path.exists(arguments.builddir):
        shutil.rmtree(arguments.builddir)

    if not os.path.exists(arguments.builddir):
        os.makedirs(arguments.builddir)

def configure_jerry(arguments):
    configure_output_dir(arguments)

    build_options = generate_build_options(arguments)

    cmake_cmd = ['cmake', '-B' + arguments.builddir, '-H' + settings.PROJECT_DIR]

    if arguments.install:
        cmake_cmd.append('-DCMAKE_INSTALL_PREFIX=%s' % arguments.install)

    cmake_cmd.extend(build_options)

    return subprocess.call(cmake_cmd)

def make_jerry(arguments):
    make_cmd = ['cmake', '--build', arguments.builddir, '--config', arguments.build_type]
    env = dict(os.environ)
    env['CMAKE_BUILD_PARALLEL_LEVEL'] = str(arguments.jobs)
    env['MAKEFLAGS'] = '-j%d' % (arguments.jobs) # Workaround for CMake < 3.12
    proc = subprocess.Popen(make_cmd, env=env)
    proc.wait()

    return proc.returncode

def install_jerry(arguments):
    install_target = 'INSTALL' if os.path.exists(os.path.join(arguments.builddir, 'Jerry.sln')) else 'install'
    make_cmd = ['cmake', '--build', arguments.builddir, '--config', arguments.build_type, '--target', install_target]
    return subprocess.call(make_cmd)

def print_result(ret):
    print('=' * 30)
    if ret:
        print('Build failed with exit code: %s' % (ret))
    else:
        print('Build succeeded!')
    print('=' * 30)

def main():
    arguments = get_arguments()

    ret = configure_jerry(arguments)

    if not ret:
        ret = make_jerry(arguments)

    if not ret and arguments.install is not None:
        ret = install_jerry(arguments)

    print_result(ret)
    sys.exit(ret)


if __name__ == "__main__":
    main()
