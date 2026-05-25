#!/usr/bin/env python3
###############################################################################
# Copyright (C) 2026  Billy Kozak                                             #
#                                                                             #
# This file is part of the jauto-profiler program                             #
#                                                                             #
# This program is free software: you can redistribute it and/or modify        #
# it under the terms of the GNU Lesser General Public License as published by #
# the Free Software Foundation, either version 3 of the License, or           #
# (at your option) any later version.                                         #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               #
# GNU Lesser General Public License for more details.                         #
#                                                                             #
# You should have received a copy of the GNU Lesser General Public License    #
# along with this program.  If not, see <http://www.gnu.org/licenses/>.       #
###############################################################################

import os
import socket
import struct

SOCKET_ENV_VAR = "JAUTO_PROF_SOCKET"
DEFAULT_SOCKET_PATH = "/tmp/jauto-profiler.sock"

_MSG_TYPE_REQUEST_LOADED_CLASSES  = 0
_MSG_TYPE_RESPONSE_LOADED_CLASSES = 1
_MSG_TYPE_REQUEST_CLASS_METHODS   = 2
_MSG_TYPE_RESPONSE_CLASS_METHODS  = 3

_HDR_FMT  = "<II"
_HDR_SIZE = struct.calcsize(_HDR_FMT)


class ProfClient:

    def __init__(self, path: str | None = None):
        if path is None:
            path = os.environ.get(SOCKET_ENV_VAR, DEFAULT_SOCKET_PATH)
        self._path = path

    def _recv_exact(self, sock: socket.socket, n: int) -> bytes:
        buf = bytearray()
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("connection closed before full message received")
            buf.extend(chunk)
        return bytes(buf)

    def _parse_string_list(self, body: bytes) -> list[str]:
        if len(body) < 4:
            raise ValueError("response body too short")

        (count,) = struct.unpack_from("<I", body, 0)
        offset = 4
        items = []

        for _ in range(count):
            if offset + 2 > len(body):
                raise ValueError("truncated entry")
            (item_len,) = struct.unpack_from("<H", body, offset)
            offset += 2
            if offset + item_len > len(body):
                raise ValueError("truncated string")
            items.append(body[offset:offset + item_len].decode("utf-8", errors="replace"))
            offset += item_len

        return items

    def _request_response(
        self,
        sock: socket.socket,
        req_type: int,
        body: bytes,
        expected_resp_type: int,
    ) -> bytes:
        hdr = struct.pack(_HDR_FMT, req_type, len(body))
        sock.sendall(hdr + body)

        resp_hdr = self._recv_exact(sock, _HDR_SIZE)
        resp_type, resp_size = struct.unpack(_HDR_FMT, resp_hdr)

        if resp_type != expected_resp_type:
            raise ValueError(f"unexpected response type {resp_type}")

        return self._recv_exact(sock, resp_size) if resp_size > 0 else b""

    def list_loaded_classes(self) -> list[str]:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(self._path)
            body = self._request_response(
                sock,
                _MSG_TYPE_REQUEST_LOADED_CLASSES,
                b"",
                _MSG_TYPE_RESPONSE_LOADED_CLASSES,
            )
        return self._parse_string_list(body)

    def get_class_methods(self, class_name: str) -> list[str]:
        name_bytes = class_name.encode("utf-8")
        req_body = struct.pack("<H", len(name_bytes)) + name_bytes
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(self._path)
            body = self._request_response(
                sock,
                _MSG_TYPE_REQUEST_CLASS_METHODS,
                req_body,
                _MSG_TYPE_RESPONSE_CLASS_METHODS,
            )
        return self._parse_string_list(body)
