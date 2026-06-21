---
description: How to use the jauto-profiler-mcp
---

# jauto-profiler MCP — Agent Skill Guide

This guide describes how to use the jauto-profiler as an agent.

You are assumed to have access to the source code of the application that you will be tasked to profile, as the point
of this profiler is that you, the agent, will use your software development capabilities to inteligently choose
profiling targets through your understanding of the source code. Profiling a closed-source application using these
tools will be possible, but difficult.

If not yet setup, guide the user through the MCP installation process according to the distribution's README. There are
also equivilant CLI tools available for each MCP function, you can use those instead of, or along with, the MCP server.

In addition to this skill document, the tool descriptions provided by MCP and help output of the CLI will also
be useful to you in determining how and why to use each tool.

---

## What this profiler does

The profiler instruments individual Java methods at runtime using JVMTI bytecode retransformation. On each
instrumented method call it records wall-clock time and call count, accumulating one snapshot per second. You select
which methods to instrument; everything else runs at full speed.

---

## Troubleshooting

- The tools communicate with the profiler-server through a unix domain socket. You may need to troubleshoot
  problems with the socket if you are seeing communication problems. The default socket path is
  `/tmp/jauto-profiler.sock`.
- The default profiler debug log path is `/tmp/jauto-prof.log`.

---

## Prerequisites — what must be true before you can use the tools

The target JVM must have been launched with two extra flags:

```
-agentpath:/path/to/jauto-profiler.so
-Xbootclasspath/a:/path/to/jauto-prof-lib.jar
```
---

## Basic workflow

1. Start the target JVM with the flags above.
2. Call `instrument_method` for each method you want to profile.
3. You may pre-queue instrumentation on classes which are not yet loaded, or wait until they are loaded and use
   `get_loaded_classes` and `get_class_methods` find the class and name of the method you want to profile.
2. The application will start paused (by default). Use `resume` to continue execution.
3. Wait at least 5–10 seconds for snapshots to accumulate.
4. Call `dump_stats` with an `output_file` path to save the raw JSON, then use `stat_summary` or a shell/Python script
   to extract meaningful numbers. **Never try to read or interpret the raw snapshot JSON by eye** — see the
   warning below.
5. Call `deinstrument_method` when done to restore the original bytecode.
6. Use `pause_threads` if you want to stop execution for a while and give yourself some time to think or setup
   instrumentation, otherwise, bytecode happens in real-time (subject to constrains imposed by the JVM).

---

## Important Environment variables:

- `JAUTO_PROF_SOCKET`: sets the path of the profiler's unix domain socket (both for tools and for the server itself).
- `JAUTO_PROF_LOG`:  sets the path where the profiler writes its debug log.
- `JAUTO_PROF_LOG_LEVEL`: sets the log level to one of OFF, WARN, INFO, DEBUG.
- `JAUTO_PROF_PAUSE_ON_START`: set to zero to disable the automatic pause on start behaviour.

---

## Line-range probes — `instrument_line`

Use `instrument_line` when you want to measure a *region* of code rather than an entire method. You supply an
entry line number and an exit line number; the profiler injects probes at those two source lines and records the
elapsed time and call count between them.

```
instrument_line(entry_class, entry_line, exit_line, exit_class=None)
→ { status, instrument_id }
```

- If `exit_class` is omitted the exit probe is placed in the same class as the entry probe.
- `instrument_id` is returned so you can later remove the probe with `deinstrument_by_id`.
- The same performance warnings apply as for `instrument_method`: avoid probing very tight inner loops.
- Line probes are useful when a method is long and you want to isolate one logical block, or when you want to
  measure the time between a call site in one class and a return point in a callee.

---

## Choosing what to instrument — a top-down strategy

You should think of profiling as a search of a problem space. Your goal is to identify an area, or areas in the target
application where the impact of software optimization will be high.

Start by inteligently picking some candidate methods in the application to start the search. It is often better to
begin at a top-level method (or methods) and then zoom in, trying to hone-in on the part of the code which is
dominating the use of the CPU.

Always remember that the methods which dominante the run-time are not always the best targets for optimization, as what
you really want to find are significant ineffeciencies. Furthermore, sometimes optimizing the performance of some
method means updating other parts of the code (by optimizing some data structure that the method operates on, for
instance).

---

## WARNING: do not eyeball raw profiler stats

The raw snapshot data produced by `dump_stats` is large, cumulative, and
requires arithmetic across many rows. **Agents must not attempt to read or
interpret the raw JSON by eye.** Mistakes in mental arithmetic on nanosecond
counters are common, systematic, and hard to detect.

Use **`stat_summary`** to get reliable per-method metrics for a time window:

```
stat_summary(class_name, method_sig, start_time, end_time)
→ { total_runs, avg_ns_per_call, ... }
```

If `stat_summary` does not give you what you need, write a short Python or
shell script that reads the dumped JSON file and computes the values
programmatically. Do not attempt the arithmetic in your head.

---

## Interpreting the stats

Snapshots are **cumulative**. A single snapshot tells you nothing useful on its
own. Always diff two snapshots to get a rate:

```python
delta_calls = snap_b["call_count"] - snap_a["call_count"]
delta_nanos = snap_b["total_nanos"] - snap_a["total_nanos"]
calls_per_sec = delta_calls / (snap_b["timestamp"] - snap_a["timestamp"])
ns_per_call   = delta_nanos / delta_calls
```

Skip the first 2–3 snapshots after instrumentation — the JIT recompiles the
retransformed class during those seconds and the numbers will be noisy.

---

## Effect of instrumentation on performance

Instrumenting a method changes its bytecode permanently (until deinstrumented).
The JIT sees new bytecode and recompiles. Two consequences:

- **Methods the JIT was inlining will no longer be inlined** after
  instrumentation. For methods called millions of times per second (e.g. tight
  inner-loop helpers), this can cause 100–350× slowdown. The profiler is
  measuring a deoptimised version of the method, not the original.
- **Methods higher in the call tree** (called thousands of times per second,
  not millions) suffer much less disruption because they were unlikely to be
  inlined anyway. These are better instrumentation targets for observing
  real-world performance.

If you see an extreme slowdown after instrumenting a leaf method, deinstrument
it and instrument its caller instead.

---

## Cross-method consistency checks

If you instrument both a method and one of its callees simultaneously, you can
cross-check the results:

- **Calls per parent call**: `callee_calls / caller_calls` should match the
  loop count or branching factor visible in the source.
- **Time attribution**: `callee_total_nanos / caller_total_nanos` tells you
  what fraction of the parent's measured time is spent in that callee. Values
  well below 100% mean the rest is either in other callees or is profiler
  overhead on the parent itself.

These checks catch instrumentation anomalies and help distinguish real hotspots
from profiler artefacts.

---

## Important constraints

- **Class names use JVM internal format**: slashes, not dots, and `$` for
  inner classes (e.g. `org/example/Outer$Inner`, not `org.example.Outer.Inner`).
- **Method signatures come from `get_class_methods`** — The descriptor syntax is precise and any deviation will cause
    the call to fail silently, but if you know how to use the method name and type information to construct the
    signature by hand you may do so.
