# J Auto-Profiler MCP

This project implements an MCP server for JVM application profiling automation.

The MCP server allows agents to instrument individual methods and extract profiling results from this instrumentation.

# Using the Profiler

- `jauto-prof-lib.jar` must be on the classpath of your target application.
- `jauto-profiler.so` must be passed to your JVM via `-agentpath:/path/to/jauto-profiler.so`.
- Run `setup` to create the Python venv and install the CLI and MCP server.

After setup, two interfaces are available:

**CLI** — `bin/jauto-prof <subcommand>` (see builtin --help for more info)

**MCP server** — run `.venv/bin/python py-src/jauto-prof-mcp.py` and configure this invocation in your agent harness.

The profiler communicates over a Unix domain socket. The default path is `/tmp/jauto-profiler.sock`, overridable
via `$JAUTO_PROF_SOCKET`. Two simultaneously running instances must each be given a distinct socket path.

# Building

Run `build-src/setup` to create the Python venv.

Run `make` to download all other dependencies and build the whole project.

Run `make dist` to produce a self-contained distribution tarball at `build/dist/jauto-prof.tar.gz`.
Extract it and run `build-src/setup` from the extracted directory to install.

Requires python3, and openjdk 8 or higher. Set `JAVA_HOME` to compile against a particular installed version of
openjdk.

# Installation

1. Extract the distribution tarball.
2. Run the `setup` script.

The agent library (jauto-profiler.so) jar file (jauto-profiler-lib.jar), CLI entry point (jauto-prof), and MCP server
entry point (jauto-prof-mcp) will now be found in the bin directory.

# Architecture

This program runs in two pieces: the MCP server and the profiler server.

The profiler server starts as a JVMTI agent. JVMTI agents are native libraries which are attached to a target JVM
by passing an argument to the Java program (see Using the Profiler).

The profiler server opens a Unix domain socket which it uses to communicate with the MCP server using a
request/response driven messaging system. The profiler server makes use of a Java library which contains the Profiler
instrumentation code and byte-code transformation logic.

Profiler instrumentation is a process of transforming the byte-code of a targeted class in order to insert calls
to the Profiler at each entry and exit point of a target method (including exception propagation). The code which
runs at each entry and exit point is written in Java and passed in as a Jar (`jauto-prof-lib.jar`) to the target's
class-path so that the profiler server can load it.

Beacuse byte-code transformation is non-trivial and the most popular byte-code transformation library is written in
Java, the profiler-server uses JNI to call a Java function which performs the byte-code transformation using the "ASM"
library. This transformation function is also contained within the `jauto-prof-lib.jar`.

# Dependencies

Requires python3 and openjdk 8 or higher to build and run.

Other dependencies are downloaded automatically by the build and setup scripts, which includes:

- FastMCP (python library).
- ASM (Java library).

