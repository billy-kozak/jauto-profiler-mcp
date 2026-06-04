# Architecture

This program runs in two pieces: the MCP server and the profiler server.

The profiler server starts as a JVMTI agent. JVMTI agents are native libraries which are attached to a target JVM
by passing an argument to the Java program. The profiler server opens a Unix domain socket which it uses to communicate
with the MCP server and CLI tools using a request/response driven messaging system.

Profiler instrumentation is a process of transforming the byte-code of a targeted class in order to insert calls
to the Profiler at each entry and exit point of a target method (including exception propagation). The code which
runs at each entry and exit point is written in Java and passed in as a Jar (`jauto-prof-lib.jar`) to the target's
class-path so that the profiler server can load it. We start a JVM thread which periodically visits each profiler
in the system and aggregates their statistics into a ring buffer which can be polled from the JVMTI agent.

Beacuse byte-code transformation is non-trivial and the most popular byte-code transformation library is written in
Java, the profiler-server uses JNI to call a Java function which performs the byte-code transformation using the "ASM"
library. This transformation function is also contained within the `jauto-prof-lib.jar`.

The profiler server itself is architected around a single threaded event queue. We rely on the single threaded
nature of the event queue for both synchronization and atomicity. Most of the state of the profiler is stored in
the struct prof_server and most mutation of the struct prof_server is performed in the event queue. This central
event queue coordinates all the various parts of the system, including the user interface socket, and the JVMTI hook
interface, and essentially acts as the 'brains' of the profiler. The profiler server runs as an JVMTI agent thread,
so that it can directly make JNI and JVMTI calls.

The CLI tools and MCP server are written in python. The MCP server runs on FastMCP.
