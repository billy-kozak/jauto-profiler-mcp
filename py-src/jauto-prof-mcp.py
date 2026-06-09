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

import json

from mcp.server.fastmcp import FastMCP

from prof_client import ProfClient, compute_stat_summary
from time_util import resolve_time_arg

###############################################################################
# MCP server
###############################################################################

mcp = FastMCP("jauto-profiler")


@mcp.tool()
def get_loaded_classes() -> list[str]:
    """Return all classes currently loaded in the attached JVM.

    Use this as the first step to find candidate classes for profiling.
    Filter the results by package or class name to locate the subsystem you
    care about. Class names are in JVM internal format (slashes, not dots;
    '$' for inner classes), which is the format required by all other tools.

    Example: "org/example/MyService$Handler"
    """
    return ProfClient().list_loaded_classes()


@mcp.tool()
def get_class_methods(class_name: str) -> list[str]:
    """Return the methods of a loaded class as 'name:descriptor' strings.

    Call this after get_loaded_classes to get the exact method signatures
    needed by instrument_method and deinstrument_method. Always copy
    signatures from this output verbatim — do not construct them by hand,
    as descriptor syntax is precise and errors will cause silent failures.

    Returns an empty list if the class has not been loaded or has no methods.

    Example output entry: "doWork:(ILjava/lang/String;)V"
    """
    return ProfClient().get_class_methods(class_name)


@mcp.tool()
def instrument_method(class_name: str, method_sig: str) -> dict:
    """Instrument a method so that entry/exit times and call counts are recorded.

    class_name: JVM internal class name (e.g. "org/example/MyClass$Inner")
    method_sig: exact signature string from get_class_methods
                in "name:descriptor" form (e.g. "doWork:(I)V")

    Returns "ok" if instrumentation was applied immediately, or "deferred" if
    the class is not yet loaded (instrumentation will be applied automatically
    when the class loads). Raises on failure, including if the method is
    already instrumented.

    IMPORTANT — call sequentially, not in parallel. Each call triggers a JVM
    retransformation; firing multiple calls simultaneously can cause failures
    if the JVM crashes between calls.

    PERFORMANCE WARNING: instrumenting a very hot method (>1M calls/sec, i.e.
    a tight inner-loop leaf) will disrupt JIT inlining and can cause 100-350x
    slowdown. Prefer to start with higher-level methods and zoom in gradually.
    """
    return ProfClient().instrument_method(class_name, method_sig)


@mcp.tool()
def get_async_errors() -> list[dict]:
    """Return all errors from the profiler's asynchronous error log.

    The log captures errors that could not be reported synchronously —
    currently, failures that occur when a deferred instrumentation is
    attempted on class load (e.g. the requested method does not exist
    in the class that loaded).

    Each entry is a dict with:
      timestamp  - Unix timestamp (seconds) when the error was recorded
      message    - human-readable error description

    Entries are returned in chronological order (oldest first). The log
    holds up to 4096 entries; oldest entries are silently evicted when
    the buffer is full. Reads are non-destructive.
    """
    return ProfClient().get_async_errors()


@mcp.tool()
def get_instrumented_methods() -> list[dict]:
    """Return all currently instrumented and deferred methods.

    Each entry is a dict with:
      class_name  - JVM internal class name
      method_sig  - method signature in "name:descriptor" form
      status      - "active" if instrumentation is applied, or "deferred"
                    if the class has not yet loaded and instrumentation will
                    be applied when it does

    Use this to audit what is currently being profiled, or to check whether
    a deferred instrumentation has been promoted to active after its class
    loaded.
    """
    return ProfClient().list_instrumented_methods()


@mcp.tool()
def deinstrument_by_id(instrument_id: int) -> None:
    """Remove instrumentation using the instrument ID returned by instrument_method.

    instrument_id: the integer ID returned by a previous instrument_method call

    Removes the instrumentation without requiring the original class name or
    method signature. Handles both active and deferred instrumentations. Use
    get_instrumented_methods to look up instrument IDs if needed.

    Raises on failure (e.g. if the ID is not found).
    """
    ProfClient().deinstrument_by_id(instrument_id)


@mcp.tool()
def deinstrument_method(class_name: str, method_sig: str) -> None:
    """Remove instrumentation from a previously instrumented method.

    class_name: JVM internal class name (e.g. "org/example/MyClass$Inner")
    method_sig: exact signature string passed to instrument_method

    Restores the original bytecode and frees the profiler slot for reuse.
    Call this when you are done analysing a method so you can move on to
    others without exhausting the slot limit. Raises on failure.
    """
    ProfClient().deinstrument_method(class_name, method_sig)


@mcp.tool()
def dump_stats(output_file: str) -> str:
    """Write profiling statistics for all instrumented methods to a JSON file.

    Each entry in the JSON contains:
      class_name  - JVM internal class name
      method_sig  - method signature in "name:descriptor" form
      snapshots   - list of per-second snapshots in chronological order,
                    each with timestamp (Unix seconds), call_count
                    (cumulative), and total_nanos (cumulative nanoseconds).

    Snapshots are CUMULATIVE — diff consecutive pairs to get rates:
      calls/sec  = (snap_b.call_count  - snap_a.call_count)
                   / (snap_b.timestamp - snap_a.timestamp)
      ns/call    = (snap_b.total_nanos - snap_a.total_nanos)
                   / (snap_b.call_count - snap_a.call_count)

    Skip the first 2-3 snapshots after instrumentation — the JIT recompiles
    during that window and the numbers will be noisy.

    Up to 3600 snapshots (one hour) of history are retained per method.

    WARNING FOR AGENTS: Do NOT attempt to read or interpret the raw JSON dump
    by eye. The snapshot data is large, cumulative, and requires arithmetic
    across many rows. Use the stat_summary tool to extract meaningful metrics,
    or write a short Python/shell script to analyse the file programmatically.
    Eyeballing snapshot tables leads to systematic errors.
    """
    stats = ProfClient().get_stats()
    with open(output_file, "w") as f:
        json.dump(stats, f, indent=2)
    return f"Stats written to {output_file}"


@mcp.tool()
def stat_summary(
    class_name: str,
    method_sig: str,
    start_time: str | float,
    end_time: str | float = 0,
) -> dict:
    """Return total runs and average ns/call for a method over a time window.

    Fetches the current profiler stats and summarises the named method for
    the window [start_time, end_time] (inclusive).

    class_name: JVM internal class name (e.g. "org/example/MyClass")
    method_sig: exact signature from get_class_methods (e.g. "doWork:(I)V")

    start_time and end_time accept two forms:
      number  — seconds in the past from now (0 = now, 60 = one minute ago)
      string  — [yyyy-][mm-][dd-]hh:mm:ss in local time, with date components
                optional from the left (missing parts default to today)

    end_time defaults to 0 (the current time).

    Returns a dict with:
      class_name      - as supplied
      method_sig      - as supplied
      start_time      - resolved window start as a Unix timestamp
      end_time        - resolved window end as a Unix timestamp
      total_runs      - total method invocations in the window
      avg_ns_per_call - average wall-clock nanoseconds per invocation

    At least two snapshots must fall within the requested window.
    Skip the first 2-3 seconds after instrumentation — JIT recompilation
    makes those samples noisy.
    """
    start = resolve_time_arg(start_time)
    end   = resolve_time_arg(end_time)
    stats = ProfClient().get_stats()
    return compute_stat_summary(stats, class_name, method_sig, start, end)


@mcp.tool()
def shutdown_jvm() -> None:
    """Request the profiled JVM to shut down cleanly via System.exit(0).

    Sends a shutdown request to the profiler server, which will call
    System.exit(0) on the target JVM. The profiler socket will close as part
    of the shutdown. Use this when profiling is complete and you want to stop
    the target application.
    """
    ProfClient().shutdown()


@mcp.tool()
def pause_threads() -> str:
    """Pause all application threads in the attached JVM.

    Suspends every non-daemon application thread so the JVM stops making
    progress. The profiler agent thread continues to run, so you can still
    issue further commands (instrument methods, collect stats, resume).

    Returns 'paused' if threads were suspended, or 'already_paused' if a
    previous pause_threads call is still in effect. Raises on error.

    Call resume_threads to let the application continue.
    """
    return ProfClient().pause_threads()


@mcp.tool()
def resume_application() -> str:
    """Resume a paused JVM.

    Handles two pause cases with a single command:
      - Startup pause: the agent pauses the JVM at VM initialization by
        default (before main() runs), giving you a window to instrument
        methods before any application code executes.
      - Runtime pause: a pause triggered by a previous pause_threads call.

    Returns 'unblocked' if the JVM was paused and is now running, or
    'nochange' if the JVM was already running. Raises on error.

    Typical workflow for one-shot applications:
      1. Start the JVM with the agent attached (it pauses automatically)
      2. Call get_loaded_classes to find classes of interest
      3. Call instrument_method for each method you want to profile
      4. Call resume_application to let main() proceed
      5. After the application exits, call get_profiler_stats

    To disable the automatic startup pause, set JAUTO_PROF_PAUSE=0 in
    the environment before launching the JVM.
    """
    return ProfClient().resume()


if __name__ == "__main__":
    mcp.run()
