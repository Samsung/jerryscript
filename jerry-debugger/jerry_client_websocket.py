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

import struct
import sys

MAX_BUFFER_SIZE = 128
WEBSOCKET_BINARY_FRAME = 2
WEBSOCKET_FIN_BIT = 0x80


if sys.version_info.major >= 3:
    # pylint: disable=invalid-name
    _ord_orig = ord
    def _ord_compat(c):
        if isinstance(c, int):
            return c
        return _ord_orig(c)
    # pylint: disable=redefined-builtin
    ord = _ord_compat


class WebSocket(object):
    def __init__(self, protocol):

        self.data_buffer = b""
        self.protocol = protocol

    def __handshake(self):
        """ Client Handshake Request. """
        self.__send_data(b"GET /jerry-debugger HTTP/1.1\r\n" +
                         b"Upgrade: websocket\r\n" +
                         b"Connection: Upgrade\r\n" +
                         b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n")

        # Expected answer from the handshake.
        expected = (b"HTTP/1.1 101 Switching Protocols\r\n" +
                    b"Upgrade: websocket\r\n" +
                    b"Connection: Upgrade\r\n" +
                    b"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")

        len_expected = len(expected)

        while len(self.data_buffer) < len_expected:
            self.data_buffer += self.protocol.receive_data()

        if self.data_buffer[0:len_expected] != expected:
            raise Exception("Unexpected handshake")

        if len(self.data_buffer) > len_expected:
            self.data_buffer = self.data_buffer[len_expected:]
        else:
            self.data_buffer = b""

    def connect(self, config_size):
        """  WebSockets connection. """
        self.protocol.connect()
        self.data_buffer = b""
        self.__handshake()

        # It will return with the Network configurations, which has the following struct:
        # header [2] - opcode[1], size[1]
        # configuration [config_size]
        len_expected = config_size + 2

        while len(self.data_buffer) < len_expected:
            self.data_buffer += self.protocol.receive_data()

        expected = struct.pack("BB",
                               WEBSOCKET_BINARY_FRAME | WEBSOCKET_FIN_BIT,
                               config_size)

        if self.data_buffer[0:2] != expected:
            raise Exception("Unexpected configuration")

        result = self.data_buffer[2:len_expected]
        self.data_buffer = self.data_buffer[len_expected:]

        return result

    def __send_data(self, data):
        """ Private function to send data using the given protocol. """
        size = len(data)

        while size > 0:
            bytes_send = self.protocol.send_data(data)
            if bytes_send < size:
                data = data[bytes_send:]
            size -= bytes_send

    def send_message(self, byte_order, packed_data):
        """ Send message. """
        message = struct.pack(byte_order + "BBI",
                              WEBSOCKET_BINARY_FRAME | WEBSOCKET_FIN_BIT,
                              WEBSOCKET_FIN_BIT + struct.unpack(byte_order + "B", packed_data[0:1])[0],
                              0) + packed_data[1:]

        self.__send_data(message)

    def close(self):
        """ Close the WebSockets connection. """
        self.protocol.close()

    def get_message(self, blocking):
        """ Receive message. """

        # Connection was closed
        if self.data_buffer is None:
            return None

        while True:
            if len(self.data_buffer) >= 2:
                if ord(self.data_buffer[0]) != WEBSOCKET_BINARY_FRAME | WEBSOCKET_FIN_BIT:
                    raise Exception("Unexpected data frame")

                size = ord(self.data_buffer[1])
                if size == 0 or size >= 126:
                    raise Exception("Unexpected data frame")

                if len(self.data_buffer) >= size + 2:
                    result = self.data_buffer[2:size + 2]
                    self.data_buffer = self.data_buffer[size + 2:]
                    return result

            if not blocking and not self.protocol.ready():
                return b''

            data = self.protocol.receive_data(MAX_BUFFER_SIZE)

            if not data:
                self.data_buffer = None
                return None
            self.data_buffer += data
