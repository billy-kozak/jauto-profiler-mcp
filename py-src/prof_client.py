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
_MSG_TYPE_REQUEST_RESUME               = 11
_MSG_TYPE_RESPONSE_RESUME              = 12
_MSG_TYPE_REQUEST_PAUSE_THREADS        = 13
_MSG_TYPE_RESPONSE_PAUSE_THREADS       = 14
_MSG_TYPE_REQUEST_LIST_INSTRUMENTED    = 15
_MSG_TYPE_RESPONSE_LIST_INSTRUMENTED   = 16
_MSG_TYPE_REQUEST_GET_ASYNC_ERRORS     = 17
_MSG_TYPE_RESPONSE_GET_ASYNC_ERRORS    = 18

_LISTED_INSTR_ACTIVE   = 0
_LISTED_INSTR_DEFERRED = 1

_RESUME_STATUS_UNBLOCKED = 0
_RESUME_STATUS_NOCHANGE  = 1
_RESUME_STATUS_ERROR     = 2

_PAUSE_THREADS_STATUS_OK             = 0
_PAUSE_THREADS_STATUS_ALREADY_PAUSED = 1
_PAUSE_THREADS_STATUS_ERROR          = 2
_PAUSE_THREADS_STATUS_RACE_FAILURE   = 3


_HDR_FMT  = "<II"
_HDR_SIZE = struct.calcsize(_HDR_FMT)

_STATUS_RESP_FMT  = _HDR_FMT + "I"   # msg_type, body_size, status
_STATUS_RESP_SIZE = struct.calcsize(_STATUS_RESP_FMT)

_PSTR_LEN_FMT  = "<H"
_PSTR_LEN_SIZE = struct.calcsize(_PSTR_LEN_FMT)

_SNAPSHOT_FMT  = "<qqq"              # timestamp, call_count, total_nanos
_SNAPSHOT_SIZE = struct.calcsize(_SNAPSHOT_FMT)


class RawMsg:

    def __init__(self, message_type: int, message_len: int, raw_body: bytes):
        self.message_type = message_type
        self.message_len = message_len
        self.raw_body = raw_body


class pstring:

    def __init__(self, buf: bytes, offset: int = 0):

        if offset + _PSTR_LEN_SIZE > len(buf):
            raise ValueError("truncated pstring length")

        (length,) = struct.unpack_from(_PSTR_LEN_FMT, buf, offset)
        start = offset + _PSTR_LEN_SIZE

        if start + length > len(buf):
            raise ValueError("truncated pstring data")

        self.value = buf[start:start + length].decode("utf-8", errors="replace")
        self.size = _PSTR_LEN_SIZE + length

    @classmethod
    def pack(cls, s: str) -> bytes:
        encoded = s.encode("utf-8")
        return struct.pack(_PSTR_LEN_FMT, len(encoded)) + encoded


class ProfClient:

    def __init__(self, path: str | None = None):
        if path is None:
            path = os.environ.get(SOCKET_ENV_VAR, DEFAULT_SOCKET_PATH)
        self._path = path
        self._sock = None
        try:
            self._connect()
        except OSError:
            pass

    def __del__(self):
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass

    def _connect(self):
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

        try:
            sock.connect(self._path)
        except OSError:
            sock.close()
            raise
        self._sock = sock

    def _recv_msg(self) -> RawMsg:
        hdr = bytearray()

        while len(hdr) < _HDR_SIZE:
            chunk = self._sock.recv(_HDR_SIZE - len(hdr))
            if not chunk:
                raise ConnectionError("connection closed before full message received")
            hdr.extend(chunk)

        message_type, body_size = struct.unpack(_HDR_FMT, bytes(hdr))
        body = bytearray()

        while len(body) < body_size:
            chunk = self._sock.recv(body_size - len(body))
            if not chunk:
                raise ConnectionError("connection closed before full message received")
            body.extend(chunk)

        return RawMsg(message_type, body_size, bytes(body))

    def _parse_string_list(self, body: bytes) -> list[str]:
        if len(body) < 4:
            raise ValueError("response body too short")

        (count,) = struct.unpack_from("<I", body, 0)
        offset = 4
        items = []

        for _ in range(count):
            ps = pstring(body, offset)
            items.append(ps.value)
            offset += ps.size

        return items

    def _send_request(
        self,
        req_type: int,
        body: bytes = b"",
    ) -> RawMsg:

        hdr = struct.pack(_HDR_FMT, req_type, len(body))

        if self._sock is None:
            try:
                self._connect()
            except OSError as e:
                raise ConnectionError(
                    f"cannot connect to profiler at {self._path}: {e}"
                ) from e

        try:
            self._sock.sendall(hdr + body)
            return self._recv_msg()
        except (OSError, ConnectionError):
            try:
                self._connect()
            except OSError as e:
                raise ConnectionError(
                    f"cannot connect to profiler at {self._path}: {e}"
                ) from e
            self._sock.sendall(hdr + body)
            return self._recv_msg()

    def _parse_stats(self, body: bytes) -> list[dict]:
        if len(body) < 4:
            raise ValueError("stats response too short")

        (count,) = struct.unpack_from("<I", body, 0)
        offset = 4
        result = []

        for _ in range(count):
            class_ps = pstring(body, offset)
            class_name = class_ps.value
            offset += class_ps.size

            sig_ps = pstring(body, offset)
            method_sig = sig_ps.value
            offset += sig_ps.size

            (snap_count,) = struct.unpack_from("<I", body, offset)
            offset += 4

            snapshots = []

            for _ in range(snap_count):
                ts, call_count, total_nanos = struct.unpack_from(
                    _SNAPSHOT_FMT, body, offset
                )
                offset += _SNAPSHOT_SIZE
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

        resp = self._send_request(
            _MSG_TYPE_REQUEST_GET_STATS,
        )

        if resp.message_type != _MSG_TYPE_RESPONSE_GET_STATS:
            raise ValueError("Unexpected response")

        return self._parse_stats(resp.raw_body)

    def list_loaded_classes(self) -> list[str]:
        resp = self._send_request(
            _MSG_TYPE_REQUEST_LOADED_CLASSES,
        )

        if resp.message_type != _MSG_TYPE_RESPONSE_LOADED_CLASSES:
            raise ValueError("Unexpected response")

        return self._parse_string_list(resp.raw_body)

    def instrument_method(self, class_name: str, method_sig: str) -> dict:

        body = pstring.pack(class_name) + pstring.pack(method_sig)

        resp = self._send_request(
            _MSG_TYPE_REQUEST_INSTRUMENT_METHOD,
            body
        )

        if resp.message_type != _MSG_TYPE_RESPONSE_INSTRUMENT_METHOD:
            raise ValueError("Unexpected response")

        status, instrument_id = struct.unpack("<IQ", resp.raw_body)

        if status == 1:
            raise RuntimeError("method is already instrumented")

        if status == 3:
            return {"status": "deferred", "instrument_id": instrument_id}

        if status != 0:
            raise RuntimeError("instrument_method failed")

        return {"status": "ok", "instrument_id": instrument_id}

    def deinstrument_method(self, class_name: str, method_sig: str) -> None:

        body = pstring.pack(class_name) + pstring.pack(method_sig)

        resp = self._send_request(
            _MSG_TYPE_REQUEST_DEINSTRUMENT_METHOD,
            body
        )

        if resp.message_type != _MSG_TYPE_RESPONSE_DEINSTRUMENT_METHOD:
            raise ValueError("Unexpected response")

        (status,) = struct.unpack("<I", resp.raw_body)

        if status != 0:
            raise RuntimeError("deinstrument_method failed")

    def shutdown(self) -> None:
        if self._sock is None:
            try:
                self._connect()
            except OSError as e:
                raise ConnectionError(
                    f"cannot connect to profiler at {self._path}: {e}"
                ) from e
        self._sock.sendall(struct.pack(_HDR_FMT, _MSG_TYPE_REQUEST_SHUTDOWN, 0))

    def resume(self) -> str:

        resp = self._send_request(
            _MSG_TYPE_REQUEST_RESUME,
        )

        if resp.message_type != _MSG_TYPE_RESPONSE_RESUME:
            raise ValueError("Unexpected response")

        (status,) = struct.unpack("<I", resp.raw_body)

        if status == _RESUME_STATUS_ERROR:
            raise RuntimeError("resume failed with error status")

        return "unblocked" if status == _RESUME_STATUS_UNBLOCKED else "nochange"

    def pause_threads(self) -> str:

        resp = self._send_request(_MSG_TYPE_REQUEST_PAUSE_THREADS)

        if resp.message_type != _MSG_TYPE_RESPONSE_PAUSE_THREADS:
            raise ValueError("Unexpected response")

        (status,) = struct.unpack("<I", resp.raw_body)

        if status == _PAUSE_THREADS_STATUS_ERROR:
            raise RuntimeError("pause_threads failed")

        if status == _PAUSE_THREADS_STATUS_RACE_FAILURE:
            raise RuntimeError(
                "pause_threads failed: concurrent thread creation "
                "prevented a stable pause"
            )

        return "paused" if status == _PAUSE_THREADS_STATUS_OK else "already_paused"

    def list_instrumented_methods(self) -> list[dict]:

        resp = self._send_request(_MSG_TYPE_REQUEST_LIST_INSTRUMENTED)

        if resp.message_type != _MSG_TYPE_RESPONSE_LIST_INSTRUMENTED:
            raise ValueError("Unexpected response")

        body = resp.raw_body
        if len(body) < 4:
            raise ValueError("response body too short")

        (count,) = struct.unpack_from("<I", body, 0)
        offset = 4
        result = []

        for _ in range(count):
            if offset + 4 > len(body):
                raise ValueError("truncated entry status")
            (status,) = struct.unpack_from("<I", body, offset)
            offset += 4

            class_ps = pstring(body, offset)
            offset += class_ps.size

            sig_ps = pstring(body, offset)
            offset += sig_ps.size

            result.append({
                "class_name": class_ps.value,
                "method_sig": sig_ps.value,
                "status": (
                    "deferred" if status == _LISTED_INSTR_DEFERRED
                    else "active"
                ),
            })

        return result

    def get_async_errors(self) -> list[dict]:

        resp = self._send_request(_MSG_TYPE_REQUEST_GET_ASYNC_ERRORS)

        if resp.message_type != _MSG_TYPE_RESPONSE_GET_ASYNC_ERRORS:
            raise ValueError("Unexpected response")

        body = resp.raw_body
        if len(body) < 4:
            raise ValueError("response body too short")

        (count,) = struct.unpack_from("<I", body, 0)
        offset = 4
        result = []

        for _ in range(count):
            if offset + 8 > len(body):
                raise ValueError("truncated timestamp")
            (timestamp,) = struct.unpack_from("<q", body, offset)
            offset += 8

            msg_ps = pstring(body, offset)
            offset += msg_ps.size

            result.append({
                "timestamp": timestamp,
                "message": msg_ps.value,
            })

        return result

    def get_class_methods(self, class_name: str) -> list[str]:

        body = pstring.pack(class_name)
        resp = self._send_request(
            _MSG_TYPE_REQUEST_CLASS_METHODS,
            body
        )

        if resp.message_type != _MSG_TYPE_RESPONSE_CLASS_METHODS:
            raise ValueError("Unexpected response")

        return self._parse_string_list(resp.raw_body)


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
