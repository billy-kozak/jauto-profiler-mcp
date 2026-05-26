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
