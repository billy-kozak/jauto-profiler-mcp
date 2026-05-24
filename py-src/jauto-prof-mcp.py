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

from mcp.server.fastmcp import FastMCP

###############################################################################
# Protocol
###############################################################################

SOCKET_ENV_VAR = "JAUTO_PROF_SOCKET"
DEFAULT_SOCKET_PATH = "/tmp/jauto-profiler.sock"

_MSG_TYPE_REQUEST_LOADED_CLASSES  = 0
_MSG_TYPE_RESPONSE_LOADED_CLASSES = 1

_HDR_FMT  = "<II"
_HDR_SIZE = struct.calcsize(_HDR_FMT)


def _socket_path() -> str:
    return os.environ.get(SOCKET_ENV_VAR, DEFAULT_SOCKET_PATH)


def _recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("connection closed before full message received")
        buf.extend(chunk)
    return bytes(buf)


def _parse_class_list(body: bytes) -> list[str]:
    if len(body) < 4:
        raise ValueError("response body too short")

    (count,) = struct.unpack_from("<I", body, 0)
    offset = 4
    classes = []

    for _ in range(count):
        if offset + 2 > len(body):
            raise ValueError("truncated class entry")
        (name_len,) = struct.unpack_from("<H", body, offset)
        offset += 2
        if offset + name_len > len(body):
            raise ValueError("truncated class name")
        classes.append(body[offset:offset + name_len].decode("utf-8", errors="replace"))
        offset += name_len

    return classes


def _fetch_loaded_classes(path: str) -> list[str]:
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(path)
        sock.sendall(struct.pack(_HDR_FMT, _MSG_TYPE_REQUEST_LOADED_CLASSES, 0))

        hdr = _recv_exact(sock, _HDR_SIZE)
        msg_type, body_size = struct.unpack(_HDR_FMT, hdr)

        if msg_type != _MSG_TYPE_RESPONSE_LOADED_CLASSES:
            raise ValueError(f"unexpected response type {msg_type}")

        body = _recv_exact(sock, body_size) if body_size > 0 else b""

    return _parse_class_list(body)


###############################################################################
# MCP server
###############################################################################

mcp = FastMCP("jauto-profiler")


@mcp.tool()
def get_loaded_classes() -> list[str]:
    """Return the list of all classes loaded by the attached JVM profiler."""
    return _fetch_loaded_classes(_socket_path())


if __name__ == "__main__":
    mcp.run()
