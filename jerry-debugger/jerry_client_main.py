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
import logging
import re
import select
import struct
import sys

# Expected debugger protocol version.
JERRY_DEBUGGER_VERSION = 9

# Messages sent by the server to client.
JERRY_DEBUGGER_CONFIGURATION = 1
JERRY_DEBUGGER_PARSE_ERROR = 2
JERRY_DEBUGGER_BYTE_CODE_CP = 3
JERRY_DEBUGGER_PARSE_FUNCTION = 4
JERRY_DEBUGGER_BREAKPOINT_LIST = 5
JERRY_DEBUGGER_BREAKPOINT_OFFSET_LIST = 6
JERRY_DEBUGGER_SOURCE_CODE = 7
JERRY_DEBUGGER_SOURCE_CODE_END = 8
JERRY_DEBUGGER_SOURCE_CODE_NAME = 9
JERRY_DEBUGGER_SOURCE_CODE_NAME_END = 10
JERRY_DEBUGGER_FUNCTION_NAME = 11
JERRY_DEBUGGER_FUNCTION_NAME_END = 12
JERRY_DEBUGGER_WAITING_AFTER_PARSE = 13
JERRY_DEBUGGER_RELEASE_BYTE_CODE_CP = 14
JERRY_DEBUGGER_MEMSTATS_RECEIVE = 15
JERRY_DEBUGGER_BREAKPOINT_HIT = 16
JERRY_DEBUGGER_EXCEPTION_HIT = 17
JERRY_DEBUGGER_EXCEPTION_STR = 18
JERRY_DEBUGGER_EXCEPTION_STR_END = 19
JERRY_DEBUGGER_BACKTRACE_TOTAL = 20
JERRY_DEBUGGER_BACKTRACE = 21
JERRY_DEBUGGER_BACKTRACE_END = 22
JERRY_DEBUGGER_EVAL_RESULT = 23
JERRY_DEBUGGER_EVAL_RESULT_END = 24
JERRY_DEBUGGER_WAIT_FOR_SOURCE = 25
JERRY_DEBUGGER_OUTPUT_RESULT = 26
JERRY_DEBUGGER_OUTPUT_RESULT_END = 27
JERRY_DEBUGGER_SCOPE_CHAIN = 28
JERRY_DEBUGGER_SCOPE_CHAIN_END = 29
JERRY_DEBUGGER_SCOPE_VARIABLES = 30
JERRY_DEBUGGER_SCOPE_VARIABLES_END = 31
JERRY_DEBUGGER_CLOSE_CONNECTION = 32

# Debugger option flags
JERRY_DEBUGGER_LITTLE_ENDIAN = 0x1

# Subtypes of eval
JERRY_DEBUGGER_EVAL_EVAL = "\0"
JERRY_DEBUGGER_EVAL_THROW = "\1"
JERRY_DEBUGGER_EVAL_ABORT = "\2"

# Subtypes of eval result
JERRY_DEBUGGER_EVAL_OK = 1
JERRY_DEBUGGER_EVAL_ERROR = 2

# Subtypes of output
JERRY_DEBUGGER_OUTPUT_OK = 1
JERRY_DEBUGGER_OUTPUT_ERROR = 2
JERRY_DEBUGGER_OUTPUT_WARNING = 3
JERRY_DEBUGGER_OUTPUT_DEBUG = 4
JERRY_DEBUGGER_OUTPUT_TRACE = 5


# Messages sent by the client to server.
JERRY_DEBUGGER_FREE_BYTE_CODE_CP = 1
JERRY_DEBUGGER_UPDATE_BREAKPOINT = 2
JERRY_DEBUGGER_EXCEPTION_CONFIG = 3
JERRY_DEBUGGER_PARSER_CONFIG = 4
JERRY_DEBUGGER_MEMSTATS = 5
JERRY_DEBUGGER_STOP = 6
JERRY_DEBUGGER_PARSER_RESUME = 7
JERRY_DEBUGGER_CLIENT_SOURCE = 8
JERRY_DEBUGGER_CLIENT_SOURCE_PART = 9
JERRY_DEBUGGER_NO_MORE_SOURCES = 10
JERRY_DEBUGGER_CONTEXT_RESET = 11
JERRY_DEBUGGER_CONTINUE = 12
JERRY_DEBUGGER_STEP = 13
JERRY_DEBUGGER_NEXT = 14
JERRY_DEBUGGER_FINISH = 15
JERRY_DEBUGGER_GET_BACKTRACE = 16
JERRY_DEBUGGER_EVAL = 17
JERRY_DEBUGGER_EVAL_PART = 18
JERRY_DEBUGGER_GET_SCOPE_CHAIN = 19
JERRY_DEBUGGER_GET_SCOPE_VARIABLES = 20

JERRY_DEBUGGER_SCOPE_WITH = 1
JERRY_DEBUGGER_SCOPE_LOCAL = 2
JERRY_DEBUGGER_SCOPE_CLOSURE = 3
JERRY_DEBUGGER_SCOPE_GLOBAL = 4
JERRY_DEBUGGER_SCOPE_NON_CLOSURE = 5

JERRY_DEBUGGER_VALUE_NONE = 1
JERRY_DEBUGGER_VALUE_UNDEFINED = 2
JERRY_DEBUGGER_VALUE_NULL = 3
JERRY_DEBUGGER_VALUE_BOOLEAN = 4
JERRY_DEBUGGER_VALUE_NUMBER = 5
JERRY_DEBUGGER_VALUE_STRING = 6
JERRY_DEBUGGER_VALUE_FUNCTION = 7
JERRY_DEBUGGER_VALUE_ARRAY = 8
JERRY_DEBUGGER_VALUE_OBJECT = 9

def arguments_parse():
    parser = argparse.ArgumentParser(description="JerryScript debugger client")

    parser.add_argument("address", action="store", nargs="?", default="localhost:5001",
                        help="specify a unique network address for tcp connection (default: %(default)s)")
    parser.add_argument("-v", "--verbose", action="store_true", default=False,
                        help="increase verbosity (default: %(default)s)")
    parser.add_argument("--non-interactive", action="store_true", default=False,
                        help="disable stop when newline is pressed (default: %(default)s)")
    parser.add_argument("--color", action="store_true", default=False,
                        help="enable color highlighting on source commands (default: %(default)s)")
    parser.add_argument("--display", action="store", default=None, type=int,
                        help="set display range")
    parser.add_argument("--exception", action="store", default=None, type=int, choices=[0, 1],
                        help="set exception config, usage 1: [Enable] or 0: [Disable]")
    parser.add_argument("--client-source", action="store", default=[], type=str, nargs="+",
                        help="specify a javascript source file to execute")
    parser.add_argument("--channel", choices=["websocket", "rawpacket"], default="websocket",
                        help="specify the communication channel (default: %(default)s)")
    parser.add_argument("--protocol", choices=["tcp", "serial"], default="tcp",
                        help="specify the transmission protocol over the communication channel (default: %(default)s)")
    parser.add_argument("--serial-config", metavar="CONFIG_STRING", default="/dev/ttyUSB0,115200,8,N,1",
                        help="Configure parameters for serial port (default: %(default)s)")
    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(format="%(levelname)s: %(message)s", level=logging.DEBUG)
        logging.debug("Debug logging mode: ON")

    return args


if sys.version_info.major >= 3:
    # pylint: disable=invalid-name
    _ord_orig = ord
    def _ord_compat(c):
        if isinstance(c, int):
            return c
        return _ord_orig(c)
    # pylint: disable=redefined-builtin
    ord = _ord_compat


class JerryBreakpoint(object):

    def __init__(self, line, offset, function):
        self.line = line
        self.offset = offset
        self.function = function
        self.active_index = -1

    def __str__(self):
        result = self.function.source_name or "<unknown>"
        result += ":%d" % (self.line)

        if self.function.is_func:
            result += " (in "
            result += self.function.name or "function"
            result += "() at line:%d, col:%d)" % (self.function.line, self.function.column)
        return result

    def __repr__(self):
        return ("Breakpoint(line:%d, offset:%d, active_index:%d)"
                % (self.line, self.offset, self.active_index))

class JerryPendingBreakpoint(object):
    def __init__(self, line=None, source_name=None, function=None):
        self.function = function
        self.line = line
        self.source_name = source_name or ""

        self.index = -1

    def __str__(self):
        result = self.source_name
        if self.line:
            result += ":%d" % (self.line)
        else:
            result += "%s()" % (self.function)
        return result


class JerryFunction(object):
    # pylint: disable=too-many-instance-attributes,too-many-arguments
    def __init__(self, is_func, byte_code_cp, source, source_name, line, column, name, lines, offsets):
        self.is_func = bool(is_func)
        self.byte_code_cp = byte_code_cp
        self.source = re.split("\r\n|[\r\n]", source)
        self.source_name = source_name
        self.name = name
        self.lines = {}
        self.offsets = {}
        self.line = line
        self.column = column
        self.first_breakpoint_line = lines[0]
        self.first_breakpoint_offset = offsets[0]

        if len(self.source) > 1 and not self.source[-1]:
            self.source.pop()

        for i, _line in enumerate(lines):
            offset = offsets[i]
            breakpoint = JerryBreakpoint(_line, offset, self)
            self.lines[_line] = breakpoint
            self.offsets[offset] = breakpoint

    def __repr__(self):
        result = ("Function(byte_code_cp:0x%x, source_name:%r, name:%r, line:%d, column:%d { "
                  % (self.byte_code_cp, self.source_name, self.name, self.line, self.column))

        result += ','.join([str(breakpoint) for breakpoint in self.lines.values()])

        return result + " })"


class Multimap(object):

    def __init__(self):
        self.map = {}

    def get(self, key):
        if key in self.map:
            return self.map[key]
        return []

    def insert(self, key, value):
        if key in self.map:
            self.map[key].append(value)
        else:
            self.map[key] = [value]

    def delete(self, key, value):
        items = self.map[key]

        if len(items) == 1:
            del self.map[key]
        else:
            del items[items.index(value)]

    def __repr__(self):
        return "Multimap(%r)" % (self.map)


class DebuggerAction(object):
    END = 0
    WAIT = 1
    TEXT = 2
    PROMPT = 3

    def __init__(self, action_type, action_text):
        self.action_type = action_type
        self.action_text = action_text

    def get_type(self):
        return self.action_type

    def get_text(self):
        return self.action_text


class JerryDebugger(object):
    # pylint: disable=too-many-instance-attributes,too-many-statements,too-many-public-methods,no-self-use
    def __init__(self, channel):
        self.prompt = False
        self.function_list = {}
        self.source = ''
        self.source_name = ''
        self.exception_string = ''
        self.frame_index = 0
        self.scope_vars = ""
        self.scope_data = ""
        self.client_sources = []
        self.last_breakpoint_hit = None
        self.next_breakpoint_index = 0
        self.active_breakpoint_list = {}
        self.pending_breakpoint_list = {}
        self.line_list = Multimap()
        self.display = 0
        self.green = ''
        self.red = ''
        self.yellow = ''
        self.green_bg = ''
        self.yellow_bg = ''
        self.blue = ''
        self.nocolor = ''
        self.src_offset = 0
        self.src_offset_diff = 0
        self.non_interactive = False
        self.current_out = b""
        self.current_log = b""
        self.channel = channel

        config_size = 8
        # The server will send the configuration message after connection established
        # type [1]
        # configuration [1]
        # version [4]
        # max_message_size [1]
        # cpointer_size [1]
        result = self.channel.connect(config_size)

        if len(result) != config_size or ord(result[0]) != JERRY_DEBUGGER_CONFIGURATION:
            raise Exception("Unexpected configuration")

        self.little_endian = ord(result[1]) & JERRY_DEBUGGER_LITTLE_ENDIAN
        self.max_message_size = ord(result[6])
        self.cp_size = ord(result[7])

        if self.little_endian:
            self.byte_order = "<"
            logging.debug("Little-endian machine")
        else:
            self.byte_order = ">"
            logging.debug("Big-endian machine")

        if self.cp_size == 2:
            self.cp_format = "H"
        else:
            self.cp_format = "I"

        self.idx_format = "I"

        self.version = struct.unpack(self.byte_order + self.idx_format, result[2:6])[0]
        if self.version != JERRY_DEBUGGER_VERSION:
            raise Exception("Incorrect debugger version from target: %d expected: %d" %
                            (self.version, JERRY_DEBUGGER_VERSION))

        logging.debug("Compressed pointer size: %d", self.cp_size)

    def __del__(self):
        if self.channel is not None:
            self.channel.close()

    def _exec_command(self, command_id):
        message = struct.pack(self.byte_order + "BB",
                              1,
                              command_id)
        self.channel.send_message(self.byte_order, message)

    def quit(self):
        self.prompt = False
        self._exec_command(JERRY_DEBUGGER_CONTINUE)

    def set_colors(self):
        self.nocolor = '\033[0m'
        self.green = '\033[92m'
        self.red = '\033[31m'
        self.yellow = '\033[93m'
        self.green_bg = '\033[42m\033[30m'
        self.yellow_bg = '\033[43m\033[30m'
        self.blue = '\033[94m'

    def stop(self):
        self._exec_command(JERRY_DEBUGGER_STOP)

    def set_break(self, args):
        if not args:
            return "Error: Breakpoint index expected"

        if ':' in args:
            try:
                if int(args.split(':', 1)[1]) <= 0:
                    return "Error: Positive breakpoint index expected"

                return self._set_breakpoint(args, False)

            except ValueError as val_errno:
                return "Error: Positive breakpoint index expected: %s" % (val_errno)

        return self._set_breakpoint(args, False)

    def breakpoint_list(self):
        result = ''
        if self.active_breakpoint_list:
            result += "=== %sActive breakpoints %s ===\n" % (self.green_bg, self.nocolor)
            for breakpoint in self.active_breakpoint_list.values():
                result += " %d: %s\n" % (breakpoint.active_index, breakpoint)
        if self.pending_breakpoint_list:
            result += "=== %sPending breakpoints%s ===\n" % (self.yellow_bg, self.nocolor)
            for breakpoint in self.pending_breakpoint_list.values():
                result += " %d: %s (pending)\n" % (breakpoint.index, breakpoint)

        if not self.active_breakpoint_list and not self.pending_breakpoint_list:
            result += "No breakpoints\n"

        return result

    def delete(self, args):
        if not args:
            return "Error: Breakpoint index expected\n" \
                   "Delete the given breakpoint, use 'delete all|active|pending' " \
                   "to clear all the given breakpoints\n "
        elif args in ['all', 'pending', 'active']:
            if args != "pending":
                for i in self.active_breakpoint_list.values():
                    breakpoint = self.active_breakpoint_list[i.active_index]
                    del self.active_breakpoint_list[i.active_index]
                    breakpoint.active_index = -1
                    self._send_breakpoint(breakpoint)

            if args != "active":
                if self.pending_breakpoint_list:
                    self.pending_breakpoint_list.clear()
                    self._send_parser_config(0)
            return ""

        try:
            breakpoint_index = int(args)
        except ValueError as val_errno:
            return "Error: Integer number expected, %s\n" % (val_errno)

        if breakpoint_index in self.active_breakpoint_list:
            breakpoint = self.active_breakpoint_list[breakpoint_index]
            del self.active_breakpoint_list[breakpoint_index]
            breakpoint.active_index = -1
            self._send_breakpoint(breakpoint)
            return "Breakpoint %d deleted\n" % (breakpoint_index)
        elif breakpoint_index in self.pending_breakpoint_list:
            del self.pending_breakpoint_list[breakpoint_index]
            if not self.pending_breakpoint_list:
                self._send_parser_config(0)
            return "Pending breakpoint %d deleted\n" % (breakpoint_index)
        else:
            return "Error: Breakpoint %d not found\n" % (breakpoint_index)

    def next(self):
        self.prompt = False
        self._exec_command(JERRY_DEBUGGER_NEXT)

    def step(self):
        self.prompt = False
        self._exec_command(JERRY_DEBUGGER_STEP)

    def do_continue(self):
        self.prompt = False
        self._exec_command(JERRY_DEBUGGER_CONTINUE)

    def finish(self):
        self.prompt = False
        self._exec_command(JERRY_DEBUGGER_FINISH)

    def backtrace(self, args):
        max_depth = 0
        min_depth = 0
        get_total = 0

        if args:
            args = args.split(" ")
            try:
                if "t" in args:
                    get_total = 1
                    args.remove("t")

                if len(args) >= 2:
                    min_depth = int(args[0])
                    max_depth = int(args[1])
                    if max_depth <= 0 or min_depth < 0:
                        return "Error: Positive integer number expected\n"
                    if min_depth > max_depth:
                        return "Error: Start depth needs to be lower than or equal to max depth\n"
                elif len(args) >= 1:
                    max_depth = int(args[0])
                    if max_depth <= 0:
                        return "Error: Positive integer number expected\n"

            except ValueError as val_errno:
                return "Error: Positive integer number expected, %s\n" % (val_errno)

        self.frame_index = min_depth

        message = struct.pack(self.byte_order + "BB" + self.idx_format + self.idx_format + "B",
                              1 + 4 + 4 + 1,
                              JERRY_DEBUGGER_GET_BACKTRACE,
                              min_depth,
                              max_depth,
                              get_total)

        self.channel.send_message(self.byte_order, message)

        self.prompt = False
        return ""

    def eval(self, code):
        self._send_string(JERRY_DEBUGGER_EVAL_EVAL + code, JERRY_DEBUGGER_EVAL)
        self.prompt = False

    def eval_at(self, code, index):
        self._send_string(JERRY_DEBUGGER_EVAL_EVAL + code, JERRY_DEBUGGER_EVAL, index)
        self.prompt = False

    def throw(self, code):
        self._send_string(JERRY_DEBUGGER_EVAL_THROW + code, JERRY_DEBUGGER_EVAL)
        self.prompt = False

    def abort(self, args):
        self.delete("all")
        self.exception("0")  # disable the exception handler
        self._send_string(JERRY_DEBUGGER_EVAL_ABORT + args, JERRY_DEBUGGER_EVAL)
        self.prompt = False

    def restart(self):
        self._send_string(JERRY_DEBUGGER_EVAL_ABORT + "\"r353t\"", JERRY_DEBUGGER_EVAL)
        self.prompt = False

    def exception(self, args):
        try:
            enabled = int(args)
        except (ValueError, TypeError):
            enabled = -1

        if enabled not in [0, 1]:
            return "Error: Invalid input! Usage 1: [Enable] or 0: [Disable]\n"

        if enabled:
            logging.debug("Stop at exception enabled")
            self._send_exception_config(enabled)

            return "Stop at exception enabled\n"

        logging.debug("Stop at exception disabled")
        self._send_exception_config(enabled)

        return "Stop at exception disabled\n"

    def scope_chain(self):
        self.prompt = False
        self._exec_command(JERRY_DEBUGGER_GET_SCOPE_CHAIN)

    def scope_variables(self, args):
        index = 0
        if args:
            try:
                index = int(args)
                if index < 0:
                    print("Error: A non negative integer number expected")
                    return ""

            except ValueError as val_errno:
                return "Error: Non negative integer number expected, %s\n" % (val_errno)

        message = struct.pack(self.byte_order + "BB" + self.idx_format,
                              1 + 4,
                              JERRY_DEBUGGER_GET_SCOPE_VARIABLES,
                              index)

        self.channel.send_message(self.byte_order, message)

        self.prompt = False
        return ""

    def memstats(self):
        self.prompt = False
        self._exec_command(JERRY_DEBUGGER_MEMSTATS)

    def _send_string(self, args, message_type, index=0):
        args = args.encode("utf8")

        # 1: length of type byte
        # 4: length of an uint32 value
        message_header = 1 + 4

        # Add scope chain index
        if message_type == JERRY_DEBUGGER_EVAL:
            args = struct.pack(self.byte_order + "I", index) + args

        size = len(args)

        max_fragment = min(self.max_message_size - message_header, size)

        message = struct.pack(self.byte_order + "BBI",
                              max_fragment + message_header,
                              message_type,
                              size)

        if size == max_fragment:
            self.channel.send_message(self.byte_order, message + args)
            return

        self.channel.send_message(self.byte_order, message + args[0:max_fragment])
        offset = max_fragment

        if message_type == JERRY_DEBUGGER_EVAL:
            message_type = JERRY_DEBUGGER_EVAL_PART
        else:
            message_type = JERRY_DEBUGGER_CLIENT_SOURCE_PART

        # 1: length of type byte
        message_header = 1

        max_fragment = self.max_message_size - message_header
        while offset < size:
            next_fragment = min(max_fragment, size - offset)

            message = struct.pack(self.byte_order + "BB",
                                  next_fragment + message_header,
                                  message_type)

            prev_offset = offset
            offset += next_fragment

            self.channel.send_message(self.byte_order, message + args[prev_offset:offset])

    def _breakpoint_pending_exists(self, breakpoint):
        for existing_bp in self.pending_breakpoint_list.values():
            if (breakpoint.line and existing_bp.source_name == breakpoint.source_name and \
                  existing_bp.line == breakpoint.line) \
               or (not breakpoint.line and existing_bp.function == breakpoint.function):
                return True

        return False

    def _send_breakpoint(self, breakpoint):
        message = struct.pack(self.byte_order + "BBB" + self.cp_format + self.idx_format,
                              1 + 1 + self.cp_size + 4,
                              JERRY_DEBUGGER_UPDATE_BREAKPOINT,
                              int(breakpoint.active_index >= 0),
                              breakpoint.function.byte_code_cp,
                              breakpoint.offset)
        self.channel.send_message(self.byte_order, message)

    def _send_bytecode_cp(self, byte_code_cp):
        message = struct.pack(self.byte_order + "BB" + self.cp_format,
                              1 + self.cp_size,
                              JERRY_DEBUGGER_FREE_BYTE_CODE_CP,
                              byte_code_cp)
        self.channel.send_message(self.byte_order, message)

    def _send_exception_config(self, enable):
        message = struct.pack(self.byte_order + "BBB",
                              1 + 1,
                              JERRY_DEBUGGER_EXCEPTION_CONFIG,
                              enable)
        self.channel.send_message(self.byte_order, message)

    def _send_parser_config(self, enable):
        message = struct.pack(self.byte_order + "BBB",
                              1 + 1,
                              JERRY_DEBUGGER_PARSER_CONFIG,
                              enable)
        self.channel.send_message(self.byte_order, message)

    def store_client_sources(self, args):
        self.client_sources = args

    def send_client_source(self):
        # Send no more source message if there is no source
        if not self.client_sources:
            self.send_no_more_source()
            return

        path = self.client_sources.pop(0)
        if not path.lower().endswith('.js'):
            sys.exit("Error: Javascript file expected!")
            return

        with open(path, 'r') as src_file:
            content = path + "\0" + src_file.read()
            self._send_string(content, JERRY_DEBUGGER_CLIENT_SOURCE)

    def send_no_more_source(self):
        self._exec_command(JERRY_DEBUGGER_NO_MORE_SOURCES)

    # pylint: disable=too-many-branches,too-many-locals,too-many-statements,too-many-return-statements
    def process_messages(self):
        result = ""
        while True:
            data = self.channel.get_message(False)
            if not self.non_interactive:
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    sys.stdin.readline()
                    self.stop()

            if data == b'':
                action_type = DebuggerAction.PROMPT if self.prompt else DebuggerAction.WAIT
                return DebuggerAction(action_type, "")

            if not data:  # Break the while loop if there is no more data.
                return DebuggerAction(DebuggerAction.END, "")

            buffer_type = ord(data[0])
            buffer_size = len(data) -1

            logging.debug("Main buffer type: %d, message size: %d", buffer_type, buffer_size)

            if buffer_type in [JERRY_DEBUGGER_PARSE_ERROR,
                               JERRY_DEBUGGER_BYTE_CODE_CP,
                               JERRY_DEBUGGER_PARSE_FUNCTION,
                               JERRY_DEBUGGER_BREAKPOINT_LIST,
                               JERRY_DEBUGGER_SOURCE_CODE,
                               JERRY_DEBUGGER_SOURCE_CODE_END,
                               JERRY_DEBUGGER_SOURCE_CODE_NAME,
                               JERRY_DEBUGGER_SOURCE_CODE_NAME_END,
                               JERRY_DEBUGGER_FUNCTION_NAME,
                               JERRY_DEBUGGER_FUNCTION_NAME_END]:
                result = self._parse_source(data)
                if result:
                    return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type == JERRY_DEBUGGER_WAITING_AFTER_PARSE:
                self._exec_command(JERRY_DEBUGGER_PARSER_RESUME)

            elif buffer_type == JERRY_DEBUGGER_RELEASE_BYTE_CODE_CP:
                self._release_function(data)

            elif buffer_type in [JERRY_DEBUGGER_BREAKPOINT_HIT, JERRY_DEBUGGER_EXCEPTION_HIT]:
                breakpoint_data = struct.unpack(self.byte_order + self.cp_format + self.idx_format, data[1:])

                breakpoint = self._get_breakpoint(breakpoint_data)
                self.last_breakpoint_hit = breakpoint[0]

                if buffer_type == JERRY_DEBUGGER_EXCEPTION_HIT:
                    result += "Exception throw detected (to disable automatic stop type exception 0)\n"
                    if self.exception_string:
                        result += "Exception hint: %s\n" % (self.exception_string)
                        self.exception_string = ""

                if breakpoint[1]:
                    breakpoint_info = "at"
                else:
                    breakpoint_info = "around"

                if breakpoint[0].active_index >= 0:
                    breakpoint_info += " breakpoint:%s%d%s" % (self.red, breakpoint[0].active_index, self.nocolor)

                result += "Stopped %s %s\n" % (breakpoint_info, breakpoint[0])

                if self.display > 0:
                    result += self.print_source(self.display, self.src_offset)

                self.prompt = True
                return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type == JERRY_DEBUGGER_EXCEPTION_STR:
                self.exception_string += data[1:]

            elif buffer_type == JERRY_DEBUGGER_EXCEPTION_STR_END:
                self.exception_string += data[1:]

            elif buffer_type == JERRY_DEBUGGER_BACKTRACE_TOTAL:
                total = struct.unpack(self.byte_order + self.idx_format, data[1:])[0]
                result += "Total number of frames: %d\n" % (total)
                return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type in [JERRY_DEBUGGER_BACKTRACE, JERRY_DEBUGGER_BACKTRACE_END]:
                frame_index = self.frame_index

                buffer_pos = 1
                while buffer_size > 0:
                    breakpoint_data = struct.unpack(self.byte_order + self.cp_format + self.idx_format,
                                                    data[buffer_pos: buffer_pos + self.cp_size + 4])

                    breakpoint = self._get_breakpoint(breakpoint_data)

                    result += "Frame %d: %s\n" % (frame_index, breakpoint[0])

                    frame_index += 1
                    buffer_pos += self.cp_size + 4
                    buffer_size -= self.cp_size + 4

                if buffer_type == JERRY_DEBUGGER_BACKTRACE_END:
                    self.prompt = True
                else:
                    self.frame_index = frame_index

                return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type in [JERRY_DEBUGGER_EVAL_RESULT,
                                 JERRY_DEBUGGER_EVAL_RESULT_END,
                                 JERRY_DEBUGGER_OUTPUT_RESULT,
                                 JERRY_DEBUGGER_OUTPUT_RESULT_END]:

                result = self._process_incoming_text(buffer_type, data)
                return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type == JERRY_DEBUGGER_MEMSTATS_RECEIVE:

                memory_stats = struct.unpack(self.byte_order + self.idx_format * 5,
                                             data[1: 1 + 4 * 5])

                result += "Allocated bytes: %s\n" % memory_stats[0]
                result += "Byte code bytes: %s\n" % memory_stats[1]
                result += "String bytes: %s\n" % memory_stats[2]
                result += "Object bytes: %s\n" % memory_stats[3]
                result += "Property bytes: %s\n" % memory_stats[4]

                self.prompt = True
                return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type == JERRY_DEBUGGER_WAIT_FOR_SOURCE:
                self.send_client_source()

            elif buffer_type in [JERRY_DEBUGGER_SCOPE_CHAIN, JERRY_DEBUGGER_SCOPE_CHAIN_END]:
                self.scope_data = data[1:]

                if buffer_type == JERRY_DEBUGGER_SCOPE_CHAIN_END:
                    result = self._process_scope()
                    self.scope_data = ""

                    self.prompt = True

                return DebuggerAction(DebuggerAction.TEXT, result)

            elif buffer_type in [JERRY_DEBUGGER_SCOPE_VARIABLES, JERRY_DEBUGGER_SCOPE_VARIABLES_END]:
                self.scope_vars += "".join(data[1:].decode("utf8"))

                if buffer_type == JERRY_DEBUGGER_SCOPE_VARIABLES_END:
                    result = self._process_scope_variables()
                    self.scope_vars = ""

                    self.prompt = True

                return DebuggerAction(DebuggerAction.TEXT, result)

            elif JERRY_DEBUGGER_CLOSE_CONNECTION:
                return DebuggerAction(DebuggerAction.END, "")

            else:
                raise Exception("Unknown message")

    def print_source(self, line_num, offset):
        msg = ""
        last_bp = self.last_breakpoint_hit

        if not last_bp:
            return ""

        lines = last_bp.function.source
        if last_bp.function.source_name:
            msg += "Source: %s\n" % (last_bp.function.source_name)

        if line_num == 0:
            start = 0
            end = len(last_bp.function.source)
        else:
            start = max(last_bp.line - line_num, 0)
            end = min(last_bp.line + line_num - 1, len(last_bp.function.source))
            if offset:
                if start + offset < 0:
                    self.src_offset += self.src_offset_diff
                    offset += self.src_offset_diff
                elif end + offset > len(last_bp.function.source):
                    self.src_offset -= self.src_offset_diff
                    offset -= self.src_offset_diff

                start = max(start + offset, 0)
                end = min(end + offset, len(last_bp.function.source))

        for i in range(start, end):
            if i == last_bp.line - 1:
                msg += "%s%4d%s %s>%s %s\n" % (self.green, i + 1, self.nocolor, self.red, \
                                                         self.nocolor, lines[i])
            else:
                msg += "%s%4d%s   %s\n" % (self.green, i + 1, self.nocolor, lines[i])

        return msg


    # pylint: disable=too-many-branches,too-many-locals,too-many-statements
    def _parse_source(self, data):
        source_code = b""
        source_code_name = b""
        function_name = b""
        stack = [{"line": 1,
                  "column": 1,
                  "name": "",
                  "lines": [],
                  "offsets": []}]
        new_function_list = {}
        result = ""

        while True:
            if data is None:
                return "Error: connection lost during source code receiving"

            buffer_type = ord(data[0])
            buffer_size = len(data) - 1

            logging.debug("Parser buffer type: %d, message size: %d", buffer_type, buffer_size)

            if buffer_type == JERRY_DEBUGGER_PARSE_ERROR:
                logging.error("Syntax error found")
                return ""

            elif buffer_type in [JERRY_DEBUGGER_SOURCE_CODE, JERRY_DEBUGGER_SOURCE_CODE_END]:
                source_code += data[1:]

            elif buffer_type in [JERRY_DEBUGGER_SOURCE_CODE_NAME, JERRY_DEBUGGER_SOURCE_CODE_NAME_END]:
                source_code_name += data[1:]

            elif buffer_type in [JERRY_DEBUGGER_FUNCTION_NAME, JERRY_DEBUGGER_FUNCTION_NAME_END]:
                function_name += data[1:]

            elif buffer_type == JERRY_DEBUGGER_PARSE_FUNCTION:
                logging.debug("Source name: %s, function name: %s", source_code_name, function_name)

                position = struct.unpack(self.byte_order + self.idx_format + self.idx_format,
                                         data[1: 1 + 4 + 4])

                stack.append({"source": source_code.decode("utf8"),
                              "source_name": source_code_name.decode("utf8"),
                              "line": position[0],
                              "column": position[1],
                              "name": function_name.decode("utf8"),
                              "lines": [],
                              "offsets": []})
                function_name = ""

            elif buffer_type in [JERRY_DEBUGGER_BREAKPOINT_LIST, JERRY_DEBUGGER_BREAKPOINT_OFFSET_LIST]:
                name = "lines"
                if buffer_type == JERRY_DEBUGGER_BREAKPOINT_OFFSET_LIST:
                    name = "offsets"

                logging.debug("Breakpoint %s received", name)

                buffer_pos = 1
                while buffer_size > 0:
                    line = struct.unpack(self.byte_order + self.idx_format,
                                         data[buffer_pos: buffer_pos + 4])
                    stack[-1][name].append(line[0])
                    buffer_pos += 4
                    buffer_size -= 4

            elif buffer_type == JERRY_DEBUGGER_BYTE_CODE_CP:
                byte_code_cp = struct.unpack(self.byte_order + self.cp_format,
                                             data[1: 1 + self.cp_size])[0]

                logging.debug("Byte code cptr received: {0x%x}", byte_code_cp)

                func_desc = stack.pop()

                # We know the last item in the list is the general byte code.
                if not stack:
                    func_desc["source"] = source_code.decode("utf8")
                    func_desc["source_name"] = source_code_name.decode("utf8")

                function = JerryFunction(stack,
                                         byte_code_cp,
                                         func_desc["source"],
                                         func_desc["source_name"],
                                         func_desc["line"],
                                         func_desc["column"],
                                         func_desc["name"],
                                         func_desc["lines"],
                                         func_desc["offsets"])

                new_function_list[byte_code_cp] = function

                if not stack:
                    logging.debug("Parse completed.")
                    break

            elif buffer_type == JERRY_DEBUGGER_RELEASE_BYTE_CODE_CP:
                # Redefined functions are dropped during parsing.
                byte_code_cp = struct.unpack(self.byte_order + self.cp_format,
                                             data[1: 1 + self.cp_size])[0]

                if byte_code_cp in new_function_list:
                    del new_function_list[byte_code_cp]
                    self._send_bytecode_cp(byte_code_cp)
                else:
                    self._release_function(data)

            elif buffer_type in [JERRY_DEBUGGER_OUTPUT_RESULT,
                                 JERRY_DEBUGGER_OUTPUT_RESULT_END]:
                result += self._process_incoming_text(buffer_type, data)

            else:
                logging.error("Parser error!")
                raise Exception("Unexpected message")

            data = self.channel.get_message(True)

        # Copy the ready list to the global storage.
        self.function_list.update(new_function_list)

        for function in new_function_list.values():
            for line, breakpoint in function.lines.items():
                self.line_list.insert(line, breakpoint)

        # Try to set the pending breakpoints
        if self.pending_breakpoint_list:
            logging.debug("Pending breakpoints available")
            bp_list = self.pending_breakpoint_list

            for breakpoint_index, breakpoint in bp_list.items():
                source_lines = 0
                for src in new_function_list.values():
                    if (src.source_name == breakpoint.source_name or
                            src.source_name.endswith("/" + breakpoint.source_name) or
                            src.source_name.endswith("\\" + breakpoint.source_name)):
                        source_lines = len(src.source)
                        break

                if breakpoint.line:
                    if breakpoint.line <= source_lines:
                        command = breakpoint.source_name + ":" + str(breakpoint.line)
                        set_result = self._set_breakpoint(command, True)

                        if set_result:
                            result += set_result
                            del bp_list[breakpoint_index]
                elif breakpoint.function:
                    command = breakpoint.function
                    set_result = self._set_breakpoint(command, True)

                    if set_result:
                        result += set_result
                        del bp_list[breakpoint_index]

            if not bp_list:
                self._send_parser_config(0)
            return result

        logging.debug("No pending breakpoints")
        return result


    def _release_function(self, data):
        byte_code_cp = struct.unpack(self.byte_order + self.cp_format,
                                     data[1: 1 + self.cp_size])[0]

        function = self.function_list[byte_code_cp]

        for line, breakpoint in function.lines.items():
            self.line_list.delete(line, breakpoint)
            if breakpoint.active_index >= 0:
                del self.active_breakpoint_list[breakpoint.active_index]

        del self.function_list[byte_code_cp]
        self._send_bytecode_cp(byte_code_cp)
        logging.debug("Function {0x%x} byte-code released", byte_code_cp)


    def _enable_breakpoint(self, breakpoint):
        if isinstance(breakpoint, JerryPendingBreakpoint):
            if self._breakpoint_pending_exists(breakpoint):
                return "%sPending breakpoint%s already exists\n" % (self.yellow, self.nocolor)

            self.next_breakpoint_index += 1
            breakpoint.index = self.next_breakpoint_index
            self.pending_breakpoint_list[self.next_breakpoint_index] = breakpoint
            return ("%sPending breakpoint %d%s at %s\n" % (self.yellow,
                                                           breakpoint.index,
                                                           self.nocolor,
                                                           breakpoint))

        if breakpoint.active_index < 0:
            self.next_breakpoint_index += 1
            self.active_breakpoint_list[self.next_breakpoint_index] = breakpoint
            breakpoint.active_index = self.next_breakpoint_index
            self._send_breakpoint(breakpoint)

        return "%sBreakpoint %d%s at %s\n" % (self.green,
                                              breakpoint.active_index,
                                              self.nocolor,
                                              breakpoint)


    def _set_breakpoint(self, string, pending):
        line = re.match("(.*):(\\d+)$", string)
        result = ""

        if line:
            source_name = line.group(1)
            new_line = int(line.group(2))

            for breakpoint in self.line_list.get(new_line):
                func_source = breakpoint.function.source_name
                if (source_name == func_source or
                        func_source.endswith("/" + source_name) or
                        func_source.endswith("\\" + source_name)):

                    result += self._enable_breakpoint(breakpoint)

        else:
            functions_to_enable = []
            for function in self.function_list.values():
                if function.name == string:
                    functions_to_enable.append(function)

            functions_to_enable.sort(key=lambda x: x.line)

            for function in functions_to_enable:
                result += self._enable_breakpoint(function.lines[function.first_breakpoint_line])

        if not result and not pending:
            print("No breakpoint found, do you want to add a %spending breakpoint%s? (y or [n]) " % \
                  (self.yellow, self.nocolor), end='')

            ans = sys.stdin.readline()
            if ans in ['yes\n', 'y\n']:
                if not self.pending_breakpoint_list:
                    self._send_parser_config(1)

                if line:
                    breakpoint = JerryPendingBreakpoint(int(line.group(2)), line.group(1))
                else:
                    breakpoint = JerryPendingBreakpoint(function=string)
                result += self._enable_breakpoint(breakpoint)

        return result


    def _get_breakpoint(self, breakpoint_data):
        function = self.function_list[breakpoint_data[0]]
        offset = breakpoint_data[1]

        if offset in function.offsets:
            return (function.offsets[offset], True)

        if offset < function.first_breakpoint_offset:
            return (function.offsets[function.first_breakpoint_offset], False)

        nearest_offset = -1

        for current_offset in function.offsets:
            if current_offset <= offset and current_offset > nearest_offset:
                nearest_offset = current_offset

        return (function.offsets[nearest_offset], False)

    def _process_incoming_text(self, buffer_type, data):
        message = b""
        msg_type = buffer_type
        while True:
            if buffer_type in [JERRY_DEBUGGER_EVAL_RESULT_END,
                               JERRY_DEBUGGER_OUTPUT_RESULT_END]:
                subtype = ord(data[-1])
                message += data[1:-1]
                break
            else:
                message += data[1:]

            data = self.channel.get_message(True)
            buffer_type = ord(data[0])
            # Checks if the next frame would be an invalid data frame.
            # If it is not the message type, or the end type of it, an exception is thrown.
            if buffer_type not in [msg_type, msg_type + 1]:
                raise Exception("Invalid data caught")

        # Subtypes of output
        if buffer_type == JERRY_DEBUGGER_OUTPUT_RESULT_END:
            if subtype == JERRY_DEBUGGER_OUTPUT_OK:
                log_type = "%sout:%s " % (self.blue, self.nocolor)

                message = self.current_out + message
                lines = message.decode("utf8").split("\n")
                self.current_out = lines.pop().encode("utf8")

                return "".join(["%s%s\n" % (log_type, line) for line in lines])

            if subtype == JERRY_DEBUGGER_OUTPUT_DEBUG:
                log_type = "%slog:%s " % (self.yellow, self.nocolor)

                message = self.current_log + message
                lines = message.decode("utf8").split("\n")
                self.current_log = lines.pop().encode("utf8")

                return "".join(["%s%s\n" % (log_type, line) for line in lines])

            if not message.endswith("\n"):
                message += "\n"

            if subtype == JERRY_DEBUGGER_OUTPUT_WARNING:
                return "%swarning: %s%s" % (self.yellow, self.nocolor, message.decode("utf8"))
            elif subtype == JERRY_DEBUGGER_OUTPUT_ERROR:
                return "%serr: %s%s" % (self.red, self.nocolor, message.decode("utf8"))
            elif subtype == JERRY_DEBUGGER_OUTPUT_TRACE:
                return "%strace: %s%s" % (self.blue, self.nocolor, message.decode("utf8"))

        # Subtypes of eval
        self.prompt = True

        message = message.decode("utf8")
        if not message.endswith("\n"):
            message += "\n"

        if subtype == JERRY_DEBUGGER_EVAL_ERROR:
            return "Uncaught exception: %s" % (message)
        return message

    def _process_scope_variables(self):
        buff_size = len(self.scope_vars)
        buff_pos = 0

        table = [['name', 'type', 'value']]

        while buff_pos != buff_size:
            # Process name
            name_length = ord(self.scope_vars[buff_pos:buff_pos + 1])
            buff_pos += 1
            name = self.scope_vars[buff_pos:buff_pos + name_length]
            buff_pos += name_length

            # Process type
            value_type = ord(self.scope_vars[buff_pos:buff_pos + 1])

            buff_pos += 1

            value_length = ord(self.scope_vars[buff_pos:buff_pos + 1])
            buff_pos += 1
            value = self.scope_vars[buff_pos: buff_pos + value_length]
            buff_pos += value_length

            if value_type == JERRY_DEBUGGER_VALUE_UNDEFINED:
                table.append([name, 'undefined', value])
            elif value_type == JERRY_DEBUGGER_VALUE_NULL:
                table.append([name, 'Null', value])
            elif value_type == JERRY_DEBUGGER_VALUE_BOOLEAN:
                table.append([name, 'Boolean', value])
            elif value_type == JERRY_DEBUGGER_VALUE_NUMBER:
                table.append([name, 'Number', value])
            elif value_type == JERRY_DEBUGGER_VALUE_STRING:
                table.append([name, 'String', value])
            elif value_type == JERRY_DEBUGGER_VALUE_FUNCTION:
                table.append([name, 'Function', value])
            elif value_type == JERRY_DEBUGGER_VALUE_ARRAY:
                table.append([name, 'Array', '[' + value + ']'])
            elif value_type == JERRY_DEBUGGER_VALUE_OBJECT:
                table.append([name, 'Object', value])

        result = self._form_table(table)

        return result

    def _process_scope(self):
        result = ""
        table = [['level', 'type']]

        for i, level in enumerate(self.scope_data):
            if ord(level) == JERRY_DEBUGGER_SCOPE_WITH:
                table.append([str(i), 'with'])
            elif ord(level) == JERRY_DEBUGGER_SCOPE_GLOBAL:
                table.append([str(i), 'global'])
            elif ord(level) == JERRY_DEBUGGER_SCOPE_NON_CLOSURE:
                # Currently it is only marks the catch closure.
                table.append([str(i), 'catch'])
            elif ord(level) == JERRY_DEBUGGER_SCOPE_LOCAL:
                table.append([str(i), 'local'])
            elif ord(level) == JERRY_DEBUGGER_SCOPE_CLOSURE:
                table.append([str(i), 'closure'])
            else:
                raise Exception("Unexpected scope chain element")

        result = self._form_table(table)

        return result

    def _form_table(self, table):
        result = ""
        col_width = [max(len(x) for x in col) for col in zip(*table)]
        for line in table:
            result += " | ".join("{:{}}".format(x, col_width[i])
                                 for i, x in enumerate(line)) + " \n"

        return result
