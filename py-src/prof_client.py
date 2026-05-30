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
_MSG_TYPE_REQUEST_CLASS_METHODS    = 2
_MSG_TYPE_RESPONSE_CLASS_METHODS   = 3
_MSG_TYPE_REQUEST_INSTRUMENT_METHOD  = 4
_MSG_TYPE_RESPONSE_INSTRUMENT_METHOD = 5
_MSG_TYPE_REQUEST_GET_STATS            = 6
_MSG_TYPE_RESPONSE_GET_STATS           = 7
_MSG_TYPE_REQUEST_DEINSTRUMENT_METHOD  = 8
_MSG_TYPE_RESPONSE_DEINSTRUMENT_METHOD = 9
_MSG_TYPE_REQUEST_SHUTDOWN             = 10

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

    def _parse_stats(self, body: bytes) -> list[dict]:
        if len(body) < 4:
            raise ValueError("stats response too short")

        (count,) = struct.unpack_from("<I", body, 0)
        offset = 4
        result = []

        for _ in range(count):
            (class_len,) = struct.unpack_from("<H", body, offset)
            offset += 2
            class_name = body[offset:offset + class_len].decode(
                "utf-8", errors="replace"
            )
            offset += class_len

            (sig_len,) = struct.unpack_from("<H", body, offset)
            offset += 2
            method_sig = body[offset:offset + sig_len].decode(
                "utf-8", errors="replace"
            )
            offset += sig_len

            (snap_count,) = struct.unpack_from("<I", body, offset)
            offset += 4

            snapshots = []
            for _ in range(snap_count):
                ts, call_count, total_nanos = struct.unpack_from(
                    "<qqq", body, offset
                )
                offset += 24
                snapshots.append({
                    "timestamp": ts,
                    "call_count": call_count,
                    "total_nanos": total_nanos,
                })

            result.append({
                "class_name": class_name,
                "method_sig": method_sig,
                "snapshots": snapshots,
            })

        return result

    def get_stats(self) -> list[dict]:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(self._path)
            body = self._request_response(
                sock,
                _MSG_TYPE_REQUEST_GET_STATS,
                b"",
                _MSG_TYPE_RESPONSE_GET_STATS,
            )
        return self._parse_stats(body)

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

    def instrument_method(self, class_name: str, method_sig: str) -> None:
        name_bytes = class_name.encode("utf-8")
        sig_bytes = method_sig.encode("utf-8")
        req_body = (
            struct.pack("<H", len(name_bytes)) + name_bytes +
            struct.pack("<H", len(sig_bytes)) + sig_bytes
        )
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(self._path)
            body = self._request_response(
                sock,
                _MSG_TYPE_REQUEST_INSTRUMENT_METHOD,
                req_body,
                _MSG_TYPE_RESPONSE_INSTRUMENT_METHOD,
            )
        if len(body) < 4:
            raise ValueError("instrument_method response too short")
        (status,) = struct.unpack_from("<I", body, 0)
        if status == 1:
            raise RuntimeError("method is already instrumented")
        if status != 0:
            raise RuntimeError("instrument_method failed")

    def deinstrument_method(self, class_name: str, method_sig: str) -> None:
        name_bytes = class_name.encode("utf-8")
        sig_bytes = method_sig.encode("utf-8")
        req_body = (
            struct.pack("<H", len(name_bytes)) + name_bytes +
            struct.pack("<H", len(sig_bytes)) + sig_bytes
        )
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(self._path)
            body = self._request_response(
                sock,
                _MSG_TYPE_REQUEST_DEINSTRUMENT_METHOD,
                req_body,
                _MSG_TYPE_RESPONSE_DEINSTRUMENT_METHOD,
            )
        if len(body) < 4:
            raise ValueError("deinstrument_method response too short")
        (status,) = struct.unpack_from("<I", body, 0)
        if status != 0:
            raise RuntimeError("deinstrument_method failed")

    def shutdown(self) -> None:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect(self._path)
            sock.sendall(struct.pack(_HDR_FMT, _MSG_TYPE_REQUEST_SHUTDOWN, 0))

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


def compute_stat_summary(
    stats: list[dict],
    class_name: str,
    method_sig: str,
    start_time: float,
    end_time: float,
) -> dict:
    """Compute total runs and average ns/call for a method over a time window.

    stats: output of ProfClient.get_stats()
    Snapshots are cumulative; the function diffs the first and last snapshot
    that fall within [start_time, end_time].
    """
    entry = None
    for e in stats:
        if e["class_name"] == class_name and e["method_sig"] == method_sig:
            entry = e
            break
    if entry is None:
        raise ValueError(
            f"method not found in stats: {class_name} / {method_sig}"
        )

    snaps = [
        s for s in entry["snapshots"]
        if start_time <= s["timestamp"] <= end_time
    ]
    if len(snaps) < 2:
        raise ValueError(
            f"need at least 2 snapshots in [{start_time}, {end_time}], "
            f"found {len(snaps)}"
        )

    first = snaps[0]
    last = snaps[-1]
    total_runs = last["call_count"] - first["call_count"]
    total_nanos = last["total_nanos"] - first["total_nanos"]
    avg_ns = total_nanos / total_runs if total_runs > 0 else 0.0

    return {
        "class_name": class_name,
        "method_sig": method_sig,
        "start_time": start_time,
        "end_time": end_time,
        "total_runs": total_runs,
        "avg_ns_per_call": avg_ns,
    }
