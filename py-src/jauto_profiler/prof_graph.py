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

import datetime

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def _rate_series(entry: dict, start_ms: float, end_ms: float):
    """Diff consecutive snapshots of one probe into a (time, rate) series.

    entry: one item from stats["probes"], as returned by ProfClient.get_stats()
    start_ms, end_ms: inclusive window bounds in epoch milliseconds

    Snapshots are cumulative, so each point in the returned series is derived
    from the difference between two consecutive snapshots that fall inside
    the window. A single snapshot in the window contributes no points.

    Returns (times, calls_per_sec, ns_per_call), all lists of equal length,
    where times is in epoch seconds.
    """
    snaps = [
        s for s in entry["snapshots"]
        if start_ms <= s["timestamp"] <= end_ms
    ]

    times = []
    calls_per_sec = []
    ns_per_call = []

    for prev, cur in zip(snaps, snaps[1:]):
        dt_ms = cur["timestamp"] - prev["timestamp"]
        if dt_ms <= 0:
            continue

        d_calls = cur["call_count"] - prev["call_count"]
        d_nanos = cur["total_nanos"] - prev["total_nanos"]

        times.append(cur["timestamp"] / 1000.0)
        calls_per_sec.append(d_calls / (dt_ms / 1000.0))
        ns_per_call.append(d_nanos / d_calls if d_calls > 0 else 0.0)

    return times, calls_per_sec, ns_per_call


def generate_stats_graph(
    stats: dict,
    output_path: str,
    names: list[str] | None = None,
    start_time: float | None = None,
    end_time: float | None = None,
) -> str:
    """Render calls/sec and average ns/call over time to an image file.

    stats: output of ProfClient.get_stats(), or an equivalent dict loaded
           from a JSON file written by dump-stats / dump_stats
    output_path: image file path to write; the format is inferred from the
           extension (e.g. ".png", ".svg")
    names: probe names to include, as they appear in
           stats["probes"][i]["name"]; defaults to every probe in stats
    start_time, end_time: Unix timestamps in seconds bounding the window to
           graph (inclusive); default to the full range of available data

    Produces a two-panel figure: calls/sec on top, average ns/call on the
    bottom, one line per probe, sharing a time x-axis. Each point is derived
    by diffing two consecutive snapshots, so a probe needs at least two
    snapshots inside the window to contribute any points.

    Raises ValueError if a requested name is not present in stats, or if no
    probe has at least two snapshots in the requested window.

    Returns output_path.
    """
    probes = stats["probes"]

    if names is not None:
        by_name = {p["name"]: p for p in probes}
        missing = [n for n in names if n not in by_name]
        if missing:
            raise ValueError(
                f"probe(s) not found in stats: {', '.join(missing)}"
            )
        probes = [by_name[n] for n in names]

    if not probes:
        raise ValueError("no probes to graph")

    start_ms = start_time * 1000.0 if start_time is not None else float("-inf")
    end_ms   = end_time   * 1000.0 if end_time   is not None else float("inf")

    fig, (ax_calls, ax_latency) = plt.subplots(
        2, 1, sharex=True, figsize=(10, 7)
    )

    plotted = 0

    for probe in probes:
        times, calls_per_sec, ns_per_call = _rate_series(
            probe, start_ms, end_ms
        )

        if not times:
            continue

        when = [datetime.datetime.fromtimestamp(t) for t in times]
        ax_calls.plot(when, calls_per_sec, label=probe["name"])
        ax_latency.plot(when, ns_per_call, label=probe["name"])
        plotted += 1

    if plotted == 0:
        raise ValueError(
            "no probe has at least two snapshots in the requested window"
        )

    ax_calls.set_title("Profiler Stats")
    ax_calls.set_ylabel("calls / sec")
    ax_calls.grid(True, alpha=0.3)
    ax_calls.legend(fontsize="small")

    ax_latency.set_ylabel("avg ns / call")
    ax_latency.set_xlabel("time")
    ax_latency.grid(True, alpha=0.3)

    fig.autofmt_xdate()
    fig.tight_layout()
    fig.savefig(output_path)
    plt.close(fig)

    return output_path
