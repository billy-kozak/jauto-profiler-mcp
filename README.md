# J Auto-Profiler MCP

This project implements an MCP server for JVM application profiling automation.

The MCP server allows agents to instrument individual methods and extract profiling results from this instrumentation.

# Using the Profiler

- Find built artifacts in bin/
- jauto-prof-lib.jar must be on the classpath of your target application.
- jauto-profiler.so must be passed to your JVM using the argument  -agentpath:jauto-profiler.so
- Run the setup script to initialize the venv for the Python MCP server. You cn then run the MCP server directly from
  the py-src directory with .venv/bin/python3 py-src/jauto-prof-mcp.py (configure this invocation in your agent
  harness).

The profiler communicates with the MCP server over a unix domain socket. By default, this socket will be placed at
`/tmp/jauto-profiler.sock`. The socket location can be changed using the environment variable `$JAUTO_PROF_SOCKET`.
Two agents running at the same time will try to use the same socket location (and therefore fail to work correclty)
unless they are each configured with different `JAUTO_PROF_SOCKET` variables.

# Building

Run `setup` to download and setup the python environment.

Run `make` to download all other dependencies and build the whole project.

Requires python3, and openjdk 8 or higher. Set `JAVA_HOME` to compile against a particular installed version of
openjdk.

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

