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


# This file is based on work under the following copyright and permission notice:
# https://github.com/test262-utils/test262-harness-py
# test262.py, _monkeyYaml.py, parseTestRecord.py

# license of test262.py:
# Copyright 2009 the Sputnik authors.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.
# This is derived from sputnik.py, the Sputnik console test runner,
# with elements from packager.py, which is separately
# copyrighted. TODO: Refactor so there is less duplication between
# test262.py and packager.py.

# license of _packager.py:
# Copyright (c) 2012 Ecma International.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

# license of _monkeyYaml.py:
# Copyright 2014 by Sam Mikes.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

# license of parseTestRecord.py:
# Copyright 2011 by Google, Inc.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.


from __future__ import print_function

import codecs
import logging
import optparse
import os
from os import path
import platform
import re
import subprocess
import sys
import tempfile
import xml.dom.minidom
from collections import Counter

import signal
import threading
import multiprocessing

import util

#######################################################################
# based on _monkeyYaml.py
#######################################################################

M_YAML_LIST_PATTERN = re.compile(r"^\[(.*)\]$")
M_YAML_MULTILINE_LIST = re.compile(r"^ *- (.*)$")


# The timeout of each test case
TEST262_CASE_TIMEOUT = 180


def yaml_load(string):
    return my_read_dict(string.splitlines())[1]


def my_read_dict(lines, indent=""):
    dictionary = {}
    key = None
    empty_lines = 0

    while lines:
        if not lines[0].startswith(indent):
            break

        line = lines.pop(0)
        if my_is_all_spaces(line):
            empty_lines += 1
            continue

        result = re.match(r"(.*?):(.*)", line)

        if result:
            if not dictionary:
                dictionary = {}
            key = result.group(1).strip()
            value = result.group(2).strip()
            (lines, value) = my_read_value(lines, value, indent)
            dictionary[key] = value
        else:
            if dictionary and key and key in dictionary:
                char = " " if empty_lines == 0 else "\n" * empty_lines
                dictionary[key] += char + line.strip()
            else:
                raise Exception("monkeyYaml is confused at " + line)
        empty_lines = 0

    if not dictionary:
        dictionary = None

    return lines, dictionary


def my_read_value(lines, value, indent):
    if value == ">" or value == "|":
        (lines, value) = my_multiline(lines, value == "|")
        value = value + "\n"
        return (lines, value)
    if lines and not value:
        if my_maybe_list(lines[0]):
            return my_multiline_list(lines, value)
        indent_match = re.match("(" + indent + r"\s+)", lines[0])
        if indent_match:
            if ":" in lines[0]:
                return my_read_dict(lines, indent_match.group(1))
            return my_multiline(lines, False)
    return lines, my_read_one_line(value)


def my_maybe_list(value):
    return M_YAML_MULTILINE_LIST.match(value)


def my_multiline_list(lines, value):
    # assume no explcit indentor (otherwise have to parse value)
    value = []
    indent = 0
    while lines:
        line = lines.pop(0)
        leading = my_leading_spaces(line)
        if my_is_all_spaces(line):
            pass
        elif leading < indent:
            lines.insert(0, line)
            break
        else:
            indent = indent or leading
            value += [my_read_one_line(my_remove_list_header(indent, line))]
    return (lines, value)


def my_remove_list_header(indent, line):
    line = line[indent:]
    return M_YAML_MULTILINE_LIST.match(line).group(1)


def my_read_one_line(value):
    if M_YAML_LIST_PATTERN.match(value):
        return my_flow_list(value)
    elif re.match(r"^[-0-9]*$", value):
        try:
            value = int(value)
        except ValueError:
            pass
    elif re.match(r"^[-.0-9eE]*$", value):
        try:
            value = float(value)
        except ValueError:
            pass
    elif re.match(r"^('|\").*\1$", value):
        value = value[1:-1]
    return value


def my_flow_list(value):
    result = M_YAML_LIST_PATTERN.match(value)
    values = result.group(1).split(",")
    return [my_read_one_line(v.strip()) for v in values]


def my_multiline(lines, preserve_newlines=False):
    # assume no explcit indentor (otherwise have to parse value)
    value = ""
    indent = my_leading_spaces(lines[0])
    was_empty = None

    while lines:
        line = lines.pop(0)
        is_empty = my_is_all_spaces(line)

        if is_empty:
            if preserve_newlines:
                value += "\n"
        elif my_leading_spaces(line) < indent:
            lines.insert(0, line)
            break
        else:
            if preserve_newlines:
                if was_empty != None:
                    value += "\n"
            else:
                if was_empty:
                    value += "\n"
                elif was_empty is False:
                    value += " "
            value += line[(indent):]

        was_empty = is_empty

    return (lines, value)


def my_is_all_spaces(line):
    return len(line.strip()) == 0


def my_leading_spaces(line):
    return len(line) - len(line.lstrip(' '))


#######################################################################
# based on parseTestRecord.py
#######################################################################

# Matches trailing whitespace and any following blank lines.
_BLANK_LINES = r"([ \t]*[\r\n]{1,2})*"

# Matches the YAML frontmatter block.
# It must be non-greedy because test262-es2015/built-ins/Object/assign/Override.js contains a comment like yaml pattern
_YAML_PATTERN = re.compile(r"/\*---(.*?)---\*/" + _BLANK_LINES, re.DOTALL)

# Matches all known variants for the license block.
# https://github.com/tc39/test262/blob/705d78299cf786c84fa4df473eff98374de7135a/tools/lint/lib/checks/license.py
_LICENSE_PATTERN = re.compile(
    r'// Copyright( \([C]\))? (\w+) .+\. {1,2}All rights reserved\.[\r\n]{1,2}' +
    r'(' +
    r'// This code is governed by the( BSD)? license found in the LICENSE file\.' +
    r'|' +
    r'// See LICENSE for details.' +
    r'|' +
    r'// Use of this source code is governed by a BSD-style license that can be[\r\n]{1,2}' +
    r'// found in the LICENSE file\.' +
    r'|' +
    r'// See LICENSE or https://github\.com/tc39/test262/blob/master/LICENSE' +
    r')' + _BLANK_LINES, re.IGNORECASE)


def yaml_attr_parser(test_record, attrs, name, onerror=print):
    parsed = yaml_load(attrs)
    if parsed is None:
        onerror("Failed to parse yaml in name %s" % name)
        return

    for key in parsed:
        value = parsed[key]
        if key == "info":
            key = "commentary"
        test_record[key] = value

    if 'flags' in test_record:
        for flag in test_record['flags']:
            test_record[flag] = ""


def find_license(src):
    match = _LICENSE_PATTERN.search(src)
    if not match:
        return None

    return match.group(0)


def find_attrs(src):
    match = _YAML_PATTERN.search(src)
    if not match:
        return (None, None)

    return (match.group(0), match.group(1).strip())


def parse_test_record(src, name, onerror=print):
    # Find the license block.
    header = find_license(src)

    # Find the YAML frontmatter.
    (frontmatter, attrs) = find_attrs(src)

    # YAML frontmatter is required for all tests.
    if frontmatter is None:
        onerror("Missing frontmatter: %s" % name)

    # The license shuold be placed before the frontmatter and there shouldn't be
    # any extra content between the license and the frontmatter.
    if header is not None and frontmatter is not None:
        header_idx = src.index(header)
        frontmatter_idx = src.index(frontmatter)
        if header_idx > frontmatter_idx:
            onerror("Unexpected license after frontmatter: %s" % name)

        # Search for any extra test content, but ignore whitespace only or comment lines.
        extra = src[header_idx + len(header): frontmatter_idx]
        if extra and any(line.strip() and not line.lstrip().startswith("//") for line in extra.split("\n")):
            onerror(
                "Unexpected test content between license and frontmatter: %s" % name)

    # Remove the license and YAML parts from the actual test content.
    test = src
    if frontmatter is not None:
        test = test.replace(frontmatter, '')
    if header is not None:
        test = test.replace(header, '')

    test_record = {}
    test_record['header'] = header.strip() if header else ''
    test_record['test'] = test

    if attrs:
        yaml_attr_parser(test_record, attrs, name, onerror)

    # Report if the license block is missing in non-generated tests.
    if header is None and "generated" not in test_record and "hashbang" not in name:
        onerror("No license found in: %s" % name)

    return test_record


#######################################################################
# based on test262.py
#######################################################################

class Test262Error(Exception):
    def __init__(self, message):
        Exception.__init__(self)
        self.message = message


def report_error(error_string):
    raise Test262Error(error_string)


def build_options():
    result = optparse.OptionParser()
    result.add_option("--command", default=None,
                      help="The command-line to run")
    result.add_option("--tests", default=path.abspath('.'),
                      help="Path to the tests")
    result.add_option("--exclude-list", default=None,
                      help="Path to the excludelist.xml file")
    result.add_option("--cat", default=False, action="store_true",
                      help="Print packaged test code that would be run")
    result.add_option("--summary", default=False, action="store_true",
                      help="Print summary after running tests")
    result.add_option("--full-summary", default=False, action="store_true",
                      help="Print summary and test output after running tests")
    result.add_option("--strict_only", default=False, action="store_true",
                      help="Test only strict mode")
    result.add_option("--non_strict_only", default=False, action="store_true",
                      help="Test only non-strict mode")
    result.add_option("--unmarked_default", default="both",
                      help="default mode for tests of unspecified strictness")
    result.add_option("-j", "--job-count", default=None, action="store", type=int,
                      help="Number of parallel test jobs to run. In case of '0' cpu count is used.")
    result.add_option("--logname", help="Filename to save stdout to")
    result.add_option("--loglevel", default="warning",
                      help="sets log level to debug, info, warning, error, or critical")
    result.add_option("--print-handle", default="print",
                      help="Command to print from console")
    result.add_option("--list-includes", default=False, action="store_true",
                      help="List includes required by tests")
    result.add_option("--module-flag", default="-m",
                      help="List includes required by tests")
    return result


def validate_options(options):
    if not options.command:
        report_error("A --command must be specified.")
    if not path.exists(options.tests):
        report_error("Couldn't find test path '%s'" % options.tests)


def is_windows():
    actual_platform = platform.system()
    return (actual_platform == 'Windows') or (actual_platform == 'Microsoft')


class TempFile(object):

    def __init__(self, suffix="", prefix="tmp", text=False):
        self.suffix = suffix
        self.prefix = prefix
        self.text = text
        self.file_desc = None
        self.name = None
        self.is_closed = False
        self.open_file()

    def open_file(self):
        (self.file_desc, self.name) = tempfile.mkstemp(
            suffix=self.suffix,
            prefix=self.prefix,
            text=self.text)

    def write(self, string):
        os.write(self.file_desc, string.encode("utf8", "ignore"))

    def read(self):
        file_desc = open(self.name, "rb")
        result = file_desc.read()
        file_desc.close()
        return result.decode("utf8", "ignore")

    def close(self):
        if not self.is_closed:
            self.is_closed = True
            os.close(self.file_desc)

    def dispose(self):
        try:
            self.close()
            os.unlink(self.name)
        except OSError as exception:
            logging.error("Error disposing temp file: %s", str(exception))


class TestResult(object):

    def __init__(self, exit_code, stdout, stderr, case):
        self.exit_code = exit_code
        self.stdout = stdout
        self.stderr = stderr
        self.case = case

    def report_outcome(self, long_format):
        name = self.case.get_name()
        mode = self.case.get_mode()

        if self.exit_code != 0 and self.exit_code != 1:
            sys.stderr.write(u"===%s failed in %s with negative:%s===\n"
                             % (name, mode, self.case.get_negative_type()))
            self.write_output(sys.stderr)

        if self.has_unexpected_outcome():
            if self.case.is_negative():
                print("=== %s passed in %s, but was expected to fail ===" % (name, mode))
                print("--- expected error: %s ---\n" % self.case.get_negative_type())
            else:
                if long_format:
                    print("=== %s failed in %s ===" % (name, mode))
                else:
                    print("%s in %s: " % (name, mode))
            self.write_output(sys.stdout)
            if long_format:
                print("===")
        elif self.case.is_negative():
            print("%s failed in %s as expected" % (name, mode))
        else:
            print("%s passed in %s" % (name, mode))

    def write_output(self, target):
        out = self.stdout.strip()
        if out:
            target.write(u"--- output --- \n %s" % out)
        error = self.stderr.strip()
        if error:
            target.write(u"--- errors ---  \n %s" % error)

        target.write(u"\n--- exit code: %d ---\n" % self.exit_code)

    def has_failed(self):
        return self.exit_code != 0

    def async_has_failed(self):
        return 'Test262:AsyncTestComplete' not in self.stdout

    def has_unexpected_outcome(self):
        if self.case.is_negative():
            return not (self.has_failed() and self.case.negative_match(self.get_error_output()))

        if self.case.is_async_test():
            return self.async_has_failed() or self.has_failed()

        return self.has_failed()

    def get_error_output(self):
        if self.stderr:
            return self.stderr
        return self.stdout


class TestCase(object):

    def __init__(self, suite, name, full_path, strict_mode, command_template, module_flag):
        self.suite = suite
        self.name = name
        self.full_path = full_path
        self.strict_mode = strict_mode
        with open(self.full_path, "rb") as file_desc:
            self.contents = file_desc.read().decode("utf8", "ignore")
        test_record = parse_test_record(self.contents, name)
        self.test = test_record["test"]
        del test_record["test"]
        del test_record["header"]
        test_record.pop("commentary", None)    # do not throw if missing
        self.test_record = test_record
        self.command_template = command_template
        self.module_flag = module_flag

        self.validate()

    def negative_match(self, stderr):
        neg = re.compile(self.get_negative_type())
        return re.search(neg, stderr)

    def get_negative(self):
        if not self.is_negative():
            return None
        return self.test_record["negative"]

    def get_negative_type(self):
        negative = self.get_negative()
        if isinstance(negative, dict) and "type" in negative:
            return negative["type"]
        return negative

    def get_negative_phase(self):
        negative = self.get_negative()
        return negative and "phase" in negative and negative["phase"]

    def get_name(self):
        return path.join(*self.name)

    def get_mode(self):
        if self.strict_mode:
            return "strict mode"
        return "non-strict mode"

    def get_path(self):
        return self.name

    def is_negative(self):
        return 'negative' in self.test_record

    def is_only_strict(self):
        return 'onlyStrict' in self.test_record

    def is_no_strict(self):
        return 'noStrict' in self.test_record or self.is_raw()

    def is_raw(self):
        return 'raw' in self.test_record

    def is_async_test(self):
        return 'async' in self.test_record or '$DONE' in self.test

    def is_module(self):
        return 'module' in self.test_record

    def get_include_list(self):
        if self.test_record.get('includes'):
            return self.test_record['includes']
        return []

    def get_additional_includes(self):
        return '\n'.join([self.suite.get_include(include) for include in self.get_include_list()])

    def get_source(self):
        if self.is_raw():
            return self.test

        source = self.suite.get_include("sta.js") + \
            self.suite.get_include("assert.js")

        if self.is_async_test():
            source = source + \
                self.suite.get_include("timer.js") + \
                self.suite.get_include("doneprintHandle.js").replace(
                    'print', self.suite.print_handle)

        source = source + \
            self.get_additional_includes() + \
            self.test + '\n'

        if self.get_negative_phase() == "early":
            source = ("throw 'Expected an early error, but code was executed.';\n" +
                      source)

        if self.strict_mode:
            source = '"use strict";\nvar strict_mode = true;\n' + source
        else:
            # add comment line so line numbers match in both strict and non-strict version
            source = '//"no strict";\nvar strict_mode = false;\n' + source

        return source

    @staticmethod
    def instantiate_template(template, params):
        def get_parameter(match):
            key = match.group(1)
            return params.get(key, match.group(0))

        return re.sub(r"\{\{(\w+)\}\}", get_parameter, template)

    @staticmethod
    def execute(command):
        if is_windows():
            args = '%s' % command
        else:
            args = command.split(" ")
        stdout = TempFile(prefix="test262-out-")
        stderr = TempFile(prefix="test262-err-")
        try:
            logging.info("exec: %s", str(args))
            process = subprocess.Popen(
                args,
                shell=False,
                stdout=stdout.file_desc,
                stderr=stderr.file_desc
            )
            timer = threading.Timer(TEST262_CASE_TIMEOUT, process.kill)
            timer.start()
            code = process.wait()
            timer.cancel()
            out = stdout.read()
            err = stderr.read()
        finally:
            stdout.dispose()
            stderr.dispose()
        return (code, out, err)

    def run_test_in(self, tmp):
        tmp.write(self.get_source())
        tmp.close()

        if self.is_module():
            arg = self.module_flag + ' ' + tmp.name
        else:
            arg = tmp.name

        command = TestCase.instantiate_template(self.command_template, {
            'path': arg
        })

        (code, out, err) = TestCase.execute(command)
        return TestResult(code, out, err, self)

    def run(self):
        tmp = TempFile(suffix=".js", prefix="test262-")
        try:
            result = self.run_test_in(tmp)
        finally:
            tmp.dispose()
        return result

    def print_source(self):
        print(self.get_source())

    def validate(self):
        flags = self.test_record.get("flags")
        phase = self.get_negative_phase()

        if phase not in [None, False, "parse", "early", "runtime", "resolution"]:
            raise TypeError("Invalid value for negative phase: " + phase)

        if not flags:
            return

        if 'raw' in flags:
            if 'noStrict' in flags:
                raise TypeError("The `raw` flag implies the `noStrict` flag")
            elif 'onlyStrict' in flags:
                raise TypeError(
                    "The `raw` flag is incompatible with the `onlyStrict` flag")
            elif self.get_include_list():
                raise TypeError(
                    "The `raw` flag is incompatible with the `includes` tag")


def pool_init():
    """Ignore CTRL+C in the worker process."""
    signal.signal(signal.SIGINT, signal.SIG_IGN)


def test_case_run_process(case):
    return case.run()


class ProgressIndicator(object):

    def __init__(self, count):
        self.count = count
        self.succeeded = 0
        self.failed = 0
        self.failed_tests = []

    def has_run(self, result):
        result.report_outcome(True)
        if result.has_unexpected_outcome():
            self.failed += 1
            self.failed_tests.append(result)
        else:
            self.succeeded += 1


def make_plural(num):
    if num == 1:
        return (num, "")
    return (num, "s")


def percent_format(partial, total):
    return "%i test%s (%.1f%%)" % (make_plural(partial) +
                                   ((100.0 * partial)/total,))


class TestSuite(object):

    def __init__(self, options):
        self.test_root = path.join(options.tests, 'test')
        self.lib_root = path.join(options.tests, 'harness')
        self.strict_only = options.strict_only
        self.non_strict_only = options.non_strict_only
        self.unmarked_default = options.unmarked_default
        self.print_handle = options.print_handle
        self.include_cache = {}
        self.exclude_list_path = options.exclude_list
        self.module_flag = options.module_flag
        self.logf = None

    def _load_excludes(self):
        if self.exclude_list_path and os.path.exists(self.exclude_list_path):
            xml_document = xml.dom.minidom.parse(self.exclude_list_path)
            xml_tests = xml_document.getElementsByTagName("test")
            return {x.getAttribute("id") for x in xml_tests}

        return set()

    def validate(self):
        if not path.exists(self.test_root):
            report_error("No test repository found")
        if not path.exists(self.lib_root):
            report_error("No test library found")

    @staticmethod
    def is_hidden(test_path):
        return test_path.startswith('.') or test_path == 'CVS'

    @staticmethod
    def is_test_case(test_path):
        return test_path.endswith('.js') and not test_path.endswith('_FIXTURE.js')

    @staticmethod
    def should_run(rel_path, tests):
        if not tests:
            return True
        for test in tests:
            if test in rel_path:
                return True
        return False

    def get_include(self, name):
        if not name in self.include_cache:
            static = path.join(self.lib_root, name)
            if path.exists(static):
                with open(static, "rb") as file_desc:
                    contents = file_desc.read().decode("utf8", "ignore")
                    contents = re.sub(r'\r\n', '\n', contents)
                    self.include_cache[name] = contents + "\n"
            else:
                report_error("Can't find: " + static)
        return self.include_cache[name]

    def enumerate_tests(self, tests, command_template):
        exclude_list = self._load_excludes()

        logging.info("Listing tests in %s", self.test_root)
        cases = []
        for root, dirs, files in os.walk(self.test_root):
            for hidden_dir in [x for x in dirs if self.is_hidden(x)]:
                dirs.remove(hidden_dir)
            dirs.sort()
            for test_path in filter(TestSuite.is_test_case, sorted(files)):
                full_path = path.join(root, test_path)
                if full_path.startswith(self.test_root):
                    rel_path = full_path[len(self.test_root)+1:]
                else:
                    logging.warning("Unexpected path %s", full_path)
                    rel_path = full_path
                if self.should_run(rel_path, tests):
                    basename = path.basename(full_path)[:-3]
                    name = rel_path.split(path.sep)[:-1] + [basename]
                    if rel_path in exclude_list:
                        print('Excluded: ' + rel_path)
                    else:
                        if not self.non_strict_only:
                            strict_case = TestCase(self, name, full_path, True, command_template, self.module_flag)
                            if not strict_case.is_no_strict():
                                if strict_case.is_only_strict() or self.unmarked_default in ['both', 'strict']:
                                    cases.append(strict_case)
                        if not self.strict_only:
                            non_strict_case = TestCase(self, name, full_path, False, command_template, self.module_flag)
                            if not non_strict_case.is_only_strict():
                                if non_strict_case.is_no_strict() or self.unmarked_default in ['both', 'non_strict']:
                                    cases.append(non_strict_case)
        logging.info("Done listing tests")
        return cases

    def print_summary(self, progress, logfile):

        def write(string):
            if logfile:
                self.logf.write(string + "\n")
            print(string)

        print("")
        write("=== Summary ===")
        count = progress.count
        succeeded = progress.succeeded
        failed = progress.failed
        write(" - Ran %i test%s" % make_plural(count))
        if progress.failed == 0:
            write(" - All tests succeeded")
        else:
            write(" - Passed " + percent_format(succeeded, count))
            write(" - Failed " + percent_format(failed, count))
            positive = [c for c in progress.failed_tests if not c.case.is_negative()]
            negative = [c for c in progress.failed_tests if c.case.is_negative()]
            if positive:
                print("")
                write("Failed Tests")
                for result in positive:
                    write("  %s in %s" % (result.case.get_name(), result.case.get_mode()))
            if negative:
                print("")
                write("Expected to fail but passed ---")
                for result in negative:
                    write("  %s in %s" % (result.case.get_name(), result.case.get_mode()))

    def print_failure_output(self, progress, logfile):
        for result in progress.failed_tests:
            if logfile:
                self.write_log(result)
            print("")
            result.report_outcome(False)

    def run(self, command_template, tests, print_summary, full_summary, logname, job_count=1):
        if not "{{path}}" in command_template:
            command_template += " {{path}}"
        cases = self.enumerate_tests(tests, command_template)
        if not cases:
            report_error("No tests to run")
        progress = ProgressIndicator(len(cases))
        if logname:
            self.logf = codecs.open(logname, "w", encoding="utf8", errors="ignore")

        if job_count == 1:
            for case in cases:
                result = case.run()
                if logname:
                    self.write_log(result)
                progress.has_run(result)
        else:
            if job_count == 0:
                job_count = None # uses multiprocessing.cpu_count()

            pool = multiprocessing.Pool(processes=job_count, initializer=pool_init)
            try:
                for result in pool.imap(test_case_run_process, cases):
                    if logname:
                        self.write_log(result)
                    progress.has_run(result)
            except KeyboardInterrupt:
                pool.terminate()
                pool.join()

        if print_summary:
            self.print_summary(progress, logname)
            if full_summary:
                self.print_failure_output(progress, logname)
            else:
                print("")
                print("Use --full-summary to see output from failed tests")
        print("")
        return progress.failed

    def write_log(self, result):
        name = result.case.get_name()
        mode = result.case.get_mode()
        if result.has_unexpected_outcome():
            if result.case.is_negative():
                self.logf.write(
                    "=== %s passed in %s, but was expected to fail === \n" % (name, mode))
                self.logf.write("--- expected error: %s ---\n" % result.case.GetNegativeType())
                result.write_output(self.logf)
            else:
                self.logf.write("=== %s failed in %s === \n" % (name, mode))
                result.write_output(self.logf)
            self.logf.write("===\n")
        elif result.case.is_negative():
            self.logf.write("%s failed in %s as expected \n" % (name, mode))
        else:
            self.logf.write("%s passed in %s \n" % (name, mode))

    def print_source(self, tests):
        cases = self.enumerate_tests(tests, "")
        if cases:
            cases[0].print_source()

    def list_includes(self, tests):
        cases = self.enumerate_tests(tests, "")
        includes_dict = Counter()
        for case in cases:
            includes = case.get_include_list()
            includes_dict.update(includes)

        print(includes_dict)


def main():
    util.setup_stdio()
    code = 0
    parser = build_options()
    (options, args) = parser.parse_args()
    validate_options(options)

    test_suite = TestSuite(options)

    test_suite.validate()
    if options.loglevel == 'debug':
        logging.basicConfig(level=logging.DEBUG)
    elif options.loglevel == 'info':
        logging.basicConfig(level=logging.INFO)
    elif options.loglevel == 'warning':
        logging.basicConfig(level=logging.WARNING)
    elif options.loglevel == 'error':
        logging.basicConfig(level=logging.ERROR)
    elif options.loglevel == 'critical':
        logging.basicConfig(level=logging.CRITICAL)

    if options.cat:
        test_suite.print_source(args)
    elif options.list_includes:
        test_suite.list_includes(args)
    else:
        code = test_suite.run(options.command, args,
                              options.summary or options.full_summary,
                              options.full_summary,
                              options.logname,
                              options.job_count)
    return code


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Test262Error as exception:
        print("Error: %s" % exception.message)
        sys.exit(1)
