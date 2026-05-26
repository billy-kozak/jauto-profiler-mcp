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

from mcp.server.fastmcp import FastMCP

from prof_client import ProfClient

###############################################################################
# MCP server
###############################################################################

mcp = FastMCP("jauto-profiler")


@mcp.tool()
def get_loaded_classes() -> list[str]:
    """Return the list of all classes loaded by the attached JVM profiler."""
    return ProfClient().list_loaded_classes()


@mcp.tool()
def instrument_method(class_name: str, method_sig: str) -> None:
    """Instrument a method for profiling.

    class_name: JVM internal class name (e.g. "app/example/MyClass")
    method_sig: method signature as returned by get_class_methods,
                in "name:descriptor" form (e.g. "doWork:(I)V")

    Raises on failure (including if the method is already instrumented).
    """
    ProfClient().instrument_method(class_name, method_sig)


@mcp.tool()
def deinstrument_method(class_name: str, method_sig: str) -> None:
    """Remove instrumentation from a previously instrumented method.

    class_name: JVM internal class name (e.g. "app/example/MyClass")
    method_sig: method signature in "name:descriptor" form, same value
                passed to instrument_method (e.g. "doWork:(I)V")

    Restores the original bytecode of the class and frees the profiler slot
    so it can be reused. Raises on failure.
    """
    ProfClient().deinstrument_method(class_name, method_sig)


@mcp.tool()
def get_class_methods(class_name: str) -> list[str]:
    """Return the methods of a loaded class as 'name:descriptor' strings.

    Returns an empty list if the class has not been loaded or has no methods.
    """
    return ProfClient().get_class_methods(class_name)


@mcp.tool()
def get_profiler_stats() -> list[dict]:
    """Return profiling statistics for all instrumented methods.

    Each entry contains:
      class_name  - JVM internal class name
      method_sig  - method signature in "name:descriptor" form
      snapshots   - list of per-second snapshots in chronological order,
                    each with timestamp (Unix seconds), call_count
                    (cumulative), and total_nanos (cumulative).

    Diff any two snapshots to get call rate and CPU time for that interval.
    Up to 3600 seconds (one hour) of history is retained per method.
    """
    return ProfClient().get_stats()


if __name__ == "__main__":
    mcp.run()
