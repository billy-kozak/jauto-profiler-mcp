# jauto-profiler MCP — Agent Skill Guide

This guide is written for agents using the jauto-profiler MCP server to profile
running JVM applications. It assumes the MCP server is already connected and the
profiler agent has been attached to a target JVM. It is not a developer guide.

---

## What this profiler does

The profiler instruments individual Java methods at runtime using JVMTI bytecode
retransformation. On each instrumented method call it records wall-clock time and
call count, accumulating one snapshot per second. You select which methods to
instrument; everything else runs at full speed.

---

## Prerequisites — what must be true before you can use the tools

The target JVM must have been launched with two extra flags:

```
-agentpath:/path/to/jauto-profiler.so
-Xbootclasspath/a:/path/to/jauto-prof-lib.jar
```

**The `-Xbootclasspath/a:` flag is mandatory**, not optional. The profiler JAR
must be on the bootstrap classpath so it is visible to classes loaded in any
classloader — including the isolated classloaders used by frameworks such as
OSGi, Gradle, and the Renaissance benchmark suite. Placing the JAR on the
regular `-cp` classpath will cause `IllegalAccessError` when the profiler tries
to call into classes loaded by child classloaders.

Only one JVM with the agent attached can run at a time. The agent binds a Unix
domain socket at a fixed path (`/tmp/jauto-profiler.sock` by default). If you
start a second instrumented JVM while another is running, the second one will
fail to create the socket silently. **Always kill the previous instrumented JVM
before starting a new one.**

---

## Basic workflow

1. Start the target JVM with the flags above.
2. Call `get_loaded_classes` to find the class names you want to profile.
3. Call `get_class_methods` on each candidate class to get exact method
   signatures.
4. Call `instrument_method` for each method you want to profile.
5. Wait at least 5–10 seconds for snapshots to accumulate.
6. Call `get_profiler_stats` with an `output_file` path, then analyse the
   resulting JSON with shell or Python tools for accurate results.
7. Call `deinstrument_method` when done to restore the original bytecode.

---

## Choosing what to instrument — a top-down strategy

Start broad, then zoom in:

1. Instrument the top-level method of the subsystem you care about (e.g. the
   main request handler or the outer loop).
2. Confirm it shows meaningful CPU time.
3. Instrument its direct callees to find which one dominates.
4. Repeat down the call tree until you reach the true hot leaf.

Do not start at the leaf — you won't know whether it matters until you have
seen the call tree above it.

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

Use `output_file` to write the raw JSON and compute the above in a Python or
shell script rather than estimating by eye. Agents are not reliable at mental
arithmetic on large numbers.

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

- **`deinstrument_method` frees the profiler slot** so it can be reused.
  Always deinstrument methods you are done with before moving on to a new set.
  Slots are not especially limited but do consume some memory. Unnecesarry profiler
  instrumentation also slows down the application.
- **Class names use JVM internal format**: slashes, not dots, and `$` for
  inner classes (e.g. `org/example/Outer$Inner`, not `org.example.Outer.Inner`).
- **Method signatures come from `get_class_methods`** — always copy them
  exactly rather than constructing them by hand. The descriptor syntax is
  precise and any deviation will cause the call to fail silently.
