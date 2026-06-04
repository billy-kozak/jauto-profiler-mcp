# J Auto-Profiler MCP

This project implements an MCP server for JVM application profiling automation.

The MCP server allows agents to instrument individual methods and extract profiling results from this instrumentation.

# Using the Profiler

- `jauto-prof-lib.jar` must be added to target JVM via `-Xbootclasspath/a:/path/to/jauto-prof-lib.jar`.
- `jauto-profiler.so` must be passed to your JVM via `-agentpath:/path/to/jauto-profiler.so`.
- Run `setup` to create the Python venv and install the CLI and MCP server.

After setup, two interfaces are available:

**CLI** — `jauto-prof <subcommand>` (see builtin --help for more info)
**MCP server** — run `jauto-prof-mcp` and configure this invocation in your agent harness.

From the dev directory, these files can be found in `scripts`. Or see [Installation](#installation) to use from a
distribution release.

The profiler communicates over a Unix domain socket. The default path is `/tmp/jauto-profiler.sock`, overridable
via `$JAUTO_PROF_SOCKET`. Two simultaneously running instances must each be given a distinct socket path.

By default, the target will pause VM execution after the profiler initializes. Set `$JAUTO_PROF_PAUSE_ON_START=0` to
disable this behaviour, otherwise, use the `resume` command to start execution of the target.

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

# Logging

The profiler server logs to /tmp/jauto-prof.log by default. Use the environment variable `$JAUTO_PROF_LOG` to change
the output location (set to empty string to log to stderr). Use `$JAUTO_PROF_LOG_LEVEL` to set logging level to one of
OFF, ERROR, WARN, INFO, DEBUG.

# Dependencies

Requires python3 and openjdk 8 or higher to build and run.

Other dependencies are downloaded automatically by the build and setup scripts, which includes:

- FastMCP (python library).
- ASM (Java library).

# Developing

See docs/DEVELOPMENT.md.
